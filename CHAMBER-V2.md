# EVERFRESH — Chamber v2: "The Vitrine"

A **warm-wood-frame, all-4-sides-glass grow cabinet on casters** — a showpiece the
_Cojoba angustifolia_ (Everfresh) tree lives in. Replaces the v1 wire-rack-and-plastic
chamber. Priorities, in Ara's order: **more room · easy daily access + beautiful ·
leak-sealing · insulation (last).**

v1 interior was 13.5×17.5×35. Species/behavior: see [everfresh-chamber-build],
[everfresh-thermal-reality], README.

---

## Design at a glance

- **Interior:** ~**24 W × 18 D × 52 H in** (~14.6 cu ft). Fits a ~3 ft tree + pot +
  lamp clearance. Wide face for viewing; shallow depth reads elegant. (Swappable to
  20×20 if you prefer square — cut list adjusts trivially.)
- **Exterior:** ~26 × 20 × ~62 in tall incl. base + top "attic" + casters.
- **Structure:** post-and-rail wood frame; **clear acrylic on all 4 faces, full-length.**
- **Front = one big swing door**, hinged **left**, handle **right**, full-height glass.
- **Top "attic"** compartment houses all electronics (exhaust fan, LED driver, Photon,
  wiring) — sealed off from the humid chamber, vented to the room. Keeps mains + drivers
  out of the wet zone.
- **Casters** (4, 2 locking) — moderately movable.

Because it's glass on all sides, **insulation is intentionally out** (can't coexist with a
vitrine, and it's your lowest priority). The all-glass glow *is* the aesthetic. Trade-off:
heat + PAR leak to the room — accepted; compensate with adequate LED wattage and keep the
west window shaded during the 4–8 PM solar spike (still the highest-leverage thermal fix).

---

## What carries over from v1

| Keep | Change |
|---|---|
| Fogger + reservoir (humidity source) | **Drop heat mat** |
| Reptile **heat bulb**, mid-level, above the reservoir, radiating up | **Drop intentional floor pool** (but keep a hidden waterproof base pan for condensation/fog-spit) |
| **Circ fan** top-mounted, pushing air **down** | **Grow light** → sleek LED (see §Lighting) |
| **Passive low intake + active top exhaust** (negative-pressure) | Enclosure: plastic-on-wire-rack → wood + acrylic vitrine |
| Vent (exhaust) fan | Access: taped flap → full swing glass door |
| 2× SHT31 — **canopy inside (control)** + **at the intake vent (gates vent)** | |
| Photon rig | |

**No firmware changes needed** — sensor roles, actuator set, and airflow direction are all
unchanged. (Verify vent constants against current `everfresh.ino` before re-enabling any
CO₂ vent schedule once the box seals tighter than v1.)

---

## Layout (bottom → top)

1. **Base cabinet (~8 in):** waterproof pan (siliconed acrylic tray or boot tray) holding
   the **fogger + reservoir**. Reachable through the front door for refills. Casters under.
2. **Growing volume:** pot on the pan; tree; **heat bulb mid-level** above the reservoir
   on a standoff, radiating up; **canopy SHT31** at leaf height.
3. **Passive intake:** low screened port on one side wall; **outside SHT31 mounted right
   at it** (reads the air the exhaust pulls in).
4. **Top of chamber:** **circ fan** blowing down (mixes the gradient); **LED** mounted to
   the underside of the top panel (driver lives in the attic above).
5. **Attic (sealed from chamber, ~6 in):** **exhaust fan** in a port cut *to the fan body*
   with a foam collar (v1's oversized hole short-circuited); **LED driver, Photon, SSR,
   wiring.** Vented to room. All cords through **sealed rubber glands.**

---

## Leak-sealing (priority #3, and the door is ~80 % of it)

- **Door perimeter:** continuous **EPDM bulb weatherstrip** + a **magnetic catch or roller
  latch** pulling it into the gasket. Full-height piano hinge on the left for rigidity.
- **Fixed panels:** set acrylic into a rabbet/groove in the frame, bedded in **100 %
  silicone** (also the glazing adhesive). Silicone every frame joint on the interior.
- **Base pan:** fully sealed, sloped to a low corner.
- **All wire pass-throughs** go through the attic via **rubber glands**, siliconed — zero
  taped holes.

---

## Lighting — sleek white LED, NOT blurple

In an all-glass case a purple panel looks clinical; **clean ~3500 K full-spectrum white**
reads as a gorgeous daylight glow and grows just as well. Understory-ish tropical → target
~**200–400 PPFD**, dimmable.

- **Recommended:** **ViparSpectra P1000** (~$100, quantum-board white, dimmable, great PAR/build).
- **Value:** Mars Hydro TS600 (~$70, slim white panel, dimmable).
- **Sleekest:** Barrina white LED bars (~$35 pack) — low-profile strips along the top; lower total PAR.

Rules: **dimmable** (fits the hormesis tuning), **driver in the attic** (humidity kills
drivers), board mounted under the top panel. LED is a **separate budget line** from the $200.

---

## Aesthetic — warm & tropical

- **Frame wood:** warm-toned + moisture-tolerant. Best value: **Douglas fir** (natural
  honey tone) or **cedar** (warm reddish, rot-resistant); **sapele/mahogany** if you find
  offcuts (true tropical look, pricier). Avoid raw poplar unless painting (greenish, not
  rot-resistant).
- **Finish:** **teak/Danish oil → spar urethane topcoat** (amber warmth + waterproofs the
  frame against the ambient humidity). This is what protects the wood in lieu of a liner.
- **Hardware:** brass hinge/handle/latch for the tropical-conservatory feel.
- Result: a lit glass conservatory cabinet — the glow + the tree are the centerpiece.

---

## Budget — ~$200 for the cabinet (LED separate)

| Item | Est. |
|---|---|
| Acrylic, 1/8" (3/16" for the door), ~30 sq ft **cut to size** (TAP Plastics, East Bay) | ~$90 |
| Frame wood — Douglas fir/cedar 2×2 posts + 1×2 rails | ~$40 |
| Casters ×4 (2 locking) | ~$18 |
| Piano hinge (48"+) + brass handle + magnetic catch/roller latch | ~$22 |
| EPDM bulb weatherstrip | ~$10 |
| Silicone (×2) + wood glue + screws/brads | ~$15 |
| Teak oil + spar urethane | ~$20 |
| **Total** | **~$215** |

Tight — trim to $200 by using a 3-cabinet-hinge set instead of a piano hinge, or 1/8"
door acrylic with a mid-rail brace. **Local source:** TAP Plastics (Emeryville/San
Leandro) cuts acrylic to exact size — critical for the 52" panels Home Depot won't cut.

---

## Cut list (24×18×52 interior; adjust for 20×20)

**Frame (Douglas fir):**
- 4× corner posts @ ~60 in (2×2)
- 8× horizontal rails: 4 @ ~24 in (front/back) + 4 @ ~18 in (sides) — top & bottom frames (1×2)
- Mid-level cross-supports for the heat-bulb standoff + intake: ~2 @ 18 in
- Base cabinet + attic framing: ~6 short members @ 18–24 in

**Acrylic (cut to size at TAP):**
- 2× sides @ ~17 × 51
- 1× back @ ~23 × 51
- 1× **door** @ ~23 × 51 (3/16")
- 1× top panel @ ~23 × 17 (clear; LED mounts under it)
- 1× base-pan floor + short walls (or a molded boot tray)

**Ports:** exhaust cut to fan body (attic) + foam collar; low intake port + screen (side wall).

---

## Build sequence (≈10-day window)

1. Order/cut acrylic at TAP (lead time — do first).
2. Mill + dry-fit the frame; cut post-and-rail joinery + panel rabbets.
3. Finish the wood (oil + spar) **before** glazing — easier, and seals end grain.
4. Build the sealed base pan; build the attic box + cut exhaust/intake ports.
5. Glaze fixed panels in silicone; hang the door with gasket + latch.
6. **Fogger leak-check by eye** before wiring.
7. Mount LED (driver in attic), circ fan, heat bulb standoff, fogger, sensors (canopy +
   intake), route all cords through glands.
8. Power up, seal-test, tune.

---

## Still to confirm

1. **Footprint:** 24×18 (my spec) vs. 20×20 square?
2. **LED pick:** P1000 (recommended) / TS600 / Barrina bars?
3. **Frame wood:** Douglas fir (cheap, honey) vs. cedar (rot-proof, reddish) vs. splurge sapele?
</content>
