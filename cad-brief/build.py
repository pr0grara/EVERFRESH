#!/usr/bin/env python3
"""Render ../CAD-BUILDER-PROMPT.md into a PDF with the zone diagram and v1 photos.

The photos are Ara's Sept 2026 shots of the v1 rig. They stay out of git
(v1-photos/ is ignored) and are pulled from the tunnel's R2 bucket when missing.
Output: out/Everfresh-CAD-brief.pdf, plus out/handoff/ (the kit Ara sends the builder:
brief text, PDF, zone diagram PNG, v1 photos, START-HERE.md with the kickoff prompt).
"""
import base64, pathlib, re, shutil, subprocess, sys

import markdown

HERE = pathlib.Path(__file__).resolve().parent
SRC = HERE.parent / "CAD-BUILDER-PROMPT.md"
PHOTOS = HERE / "v1-photos"
OUT = HERE / "out"
CHROME = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"

# R2 keys in arabuilds-code-attachments (thread 732, 2026-09-12)
PHOTO_KEYS = {
    "v1-front.jpg": "att/c1072070-0e45-441b-9263-06bb9a01177d.jpg",
    "v1-lid.jpg": "att/342b4c62-6de2-4133-8821-88dc19fd4d7e.jpg",
}
CAPTIONS = {
    "v1-front.jpg": "v1 today: a wire rack in plastic sheeting, 13.5 × 17.5 × 35 in inside. "
                    "The grow light hangs from the top; the pot sits on the lower shelf.",
    "v1-lid.jpg": "v1 lid: the controller and switching boards on a breadboard, "
                  "and the exhaust fan over its hole in the foam lid.",
}


def fetch_photos():
    PHOTOS.mkdir(exist_ok=True)
    for name, key in PHOTO_KEYS.items():
        dst = PHOTOS / name
        if dst.exists():
            continue
        raw = PHOTOS / ("raw-" + name)
        subprocess.run(["npx", "wrangler", "r2", "object", "get", f"arabuilds-code-attachments/{key}",
                        "--file", str(raw), "--remote"], cwd=HERE.parents[1] / "arabuilds", check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        subprocess.run(["sips", "-Z", "1400", str(raw), "--out", str(dst)], check=True, stdout=subprocess.DEVNULL)
        raw.unlink()


def data_uri(path, mime):
    return f"data:{mime};base64," + base64.b64encode(path.read_bytes()).decode()


def main():
    fetch_photos()
    md = SRC.read_text()
    body = markdown.markdown(md, extensions=["tables", "sane_lists"])

    svg = (HERE / "zones.svg").read_text()
    diagram = f'<section class="diagram">{svg}</section>'
    # the diagram follows the text on its own page, so no page is left half empty
    body += diagram

    photos = "".join(
        f'<figure><img src="{data_uri(PHOTOS / n, "image/jpeg")}"><figcaption>{CAPTIONS[n]}</figcaption></figure>'
        for n in PHOTO_KEYS)
    body += f'<section class="photos"><h2>Today\'s chamber (v1, September 2026)</h2><div class="pair">{photos}</div></section>'

    html = f"""<!doctype html><html><head><meta charset="utf-8"><title>Everfresh Grow Chamber: CAD brief</title>
<style>
@page {{ size: Letter; margin: 0.6in 0.65in; }}
html {{ -webkit-print-color-adjust: exact; print-color-adjust: exact; }}
body {{ font-family: -apple-system, "Helvetica Neue", Helvetica, Arial, sans-serif; font-size: 10.2pt; line-height: 1.42; color: #1f1f1c; }}
h1 {{ font-size: 22pt; margin: 0 0 4pt; letter-spacing: -0.2pt; }}
h1 + p {{ color: #6b6b66; font-size: 9.6pt; margin-top: 0; border-bottom: 1px solid #ddd; padding-bottom: 10pt; }}
h2 {{ font-size: 13.5pt; margin: 18pt 0 6pt; color: #24543a; border-bottom: 1px solid #e3e3dc; padding-bottom: 3pt; break-after: avoid; }}
h3 {{ font-size: 11.2pt; margin: 13pt 0 4pt; break-after: avoid; }}
p {{ margin: 5pt 0; }}
ul {{ margin: 4pt 0 6pt; padding-left: 16pt; }}
li {{ margin: 3pt 0; }}
table {{ border-collapse: collapse; margin: 6pt 0 8pt; font-size: 10pt; break-inside: avoid; }}
th, td {{ border: 1px solid #d6d6cf; padding: 4pt 8pt; text-align: left; vertical-align: top; }}
th {{ background: #f1f1ec; }}
section.diagram {{ break-before: page; text-align: center; }}
section.diagram svg {{ width: 100%; height: auto; max-height: 9.6in; }}
section.photos {{ break-before: page; }}
.pair {{ display: flex; gap: 14pt; align-items: flex-start; }}
figure {{ flex: 1; margin: 0; }}
figure img {{ width: 100%; border-radius: 4pt; }}
figcaption {{ font-size: 9pt; color: #55554f; margin-top: 4pt; }}
</style></head><body>{body}</body></html>"""

    OUT.mkdir(exist_ok=True)
    page = OUT / "brief.html"
    pdf = OUT / "Everfresh-CAD-brief.pdf"
    page.write_text(html)
    if pdf.exists():
        pdf.unlink()
    # Chrome writes the PDF fast but may not exit on this Mac, so a 45 s alarm ends it
    subprocess.run(["perl", "-e", "alarm 45; exec @ARGV", CHROME, "--headless=new", "--disable-gpu", "--no-sandbox",
                    "--no-first-run", "--no-default-browser-check", "--user-data-dir=/tmp/chrome-evr-brief",
                    "--no-pdf-header-footer", f"--print-to-pdf={pdf}", page.as_uri()],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not pdf.exists() or pdf.stat().st_size == 0:
        sys.exit("no PDF produced")
    print(pdf, pdf.stat().st_size)
    make_kit(pdf)


def make_kit(pdf):
    kit = OUT / "handoff"
    if kit.exists():
        shutil.rmtree(kit)
    kit.mkdir()
    shutil.copy(SRC, kit / "EVERFRESH-BRIEF.md")
    short = render_builder_brief()
    shutil.copy(short, kit / short.name)
    shutil.copy(HERE / "START-HERE.md", kit / "START-HERE.md")
    for n in PHOTO_KEYS:
        shutil.copy(PHOTOS / n, kit / n)
    png = kit / "everfresh-zones.png"
    subprocess.run(["perl", "-e", "alarm 40; exec @ARGV", CHROME, "--headless=new", "--disable-gpu", "--no-sandbox",
                    "--no-first-run", "--no-default-browser-check", "--user-data-dir=/tmp/chrome-evr-brief",
                    "--hide-scrollbars", "--force-device-scale-factor=2", "--window-size=1000,1330",
                    f"--screenshot={png}", (HERE / "zones.svg").as_uri()],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not png.exists():
        sys.exit("no diagram PNG produced")
    for f in sorted(kit.iterdir()):
        print("kit:", f.name, f.stat().st_size)


def render_builder_brief():
    """The two-page version for the human builder; the full brief is for his AI."""
    body = markdown.markdown((HERE / "BUILDER-BRIEF.md").read_text(), extensions=["sane_lists"])
    html = f"""<!doctype html><html><head><meta charset="utf-8"><title>Everfresh: the short version</title>
<style>
@page {{ size: Letter; margin: 0.55in 0.7in; }}
body {{ font-family: -apple-system, "Helvetica Neue", Helvetica, Arial, sans-serif; font-size: 10.4pt; line-height: 1.4; color: #1f1f1c; }}
h1 {{ font-size: 19pt; margin: 0 0 8pt; }}
h2 {{ font-size: 13.5pt; color: #24543a; border-bottom: 1px solid #e3e3dc; padding-bottom: 3pt; margin: 10pt 0 6pt; }}
p {{ margin: 6pt 0; }} ul {{ margin: 3pt 0 6pt; padding-left: 16pt; }} li {{ margin: 3pt 0; }}
.pagebreak {{ break-after: page; }}
</style></head><body>{body}</body></html>"""
    page = OUT / "builder-brief.html"
    pdf = OUT / "Everfresh-builder-brief.pdf"
    page.write_text(html)
    if pdf.exists():
        pdf.unlink()
    subprocess.run(["perl", "-e", "alarm 45; exec @ARGV", CHROME, "--headless=new", "--disable-gpu", "--no-sandbox",
                    "--no-first-run", "--no-default-browser-check", "--user-data-dir=/tmp/chrome-evr-brief",
                    "--no-pdf-header-footer", f"--print-to-pdf={pdf}", page.as_uri()],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not pdf.exists():
        sys.exit("no builder PDF produced")
    return pdf


if __name__ == "__main__":
    main()
