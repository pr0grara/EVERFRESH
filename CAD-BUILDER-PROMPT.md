# Everfresh Grow Chamber: CAD brief

*Handoff version, 2026-10-10. Save this in your AI assistant's workspace and ask it to read the file, or paste it in. The PDF adds a zone diagram and photos of today's chamber; START-HERE.md has a kickoff prompt.*

## The job

You're working in Rhino 9 with a skilled builder on a grow chamber: a sealed, glazed cabinet for a tropical tree, run around the clock by a small Wi-Fi controller that drives a heater, an ultrasonic fogger and two fans. The owner, Ara, has run a rough version since June 2026 and logged every minute of it. What carries forward is that life-support system: the parts, how air, heat, water and light move through the box, and the lessons behind each rule below. Improving it is part of the job.

Everything about the enclosure is open, layout included. A polished version of today's single-tree stack is welcome. So is a wider cabinet for side-by-side plantings, a small nook for cuttings, or another arrangement the builder prefers. Ara plans on single glazing in glass or acrylic (double glazing is all but out) and wants a showpiece. Budget targets: about $250 for the finished panel material and $75 for the frame. Hardware (hinges, latch, gaskets, casters, levelers, cable seals, threaded inserts) is a separate line: list it with prices for Ara to approve. Components are separate too, and Ara builds the electronics tray and wiring himself, so the builder's part there is the spaces, mounts, routes and access.

Success looks like: warm enough all winter, steady humidity with the door shut, light that actually grows the tree, no visible cable mess, a cabinet that sets up and comes apart easily, routine care that takes minutes, and something worth looking at.

## How to work

Work from the inside out. Start with the functional core alone: each component below as a simple solid at its real size, its clearance and keep-out volumes, the plants' growth envelopes, the air path, the water path and the cable routes. Show that skeleton to the builder before modeling any enclosure. Then sketch two or three enclosure concepts around it in the directions the builder picks, each with a short note on how it handles heat, sealing, water, light, cables and access. Detail only the chosen one: joinery, glazing, seals, ports, mounts, the cable harness, a cut list and shop drawings. Before handing a concept back, check that nothing intrudes on a keep-out, the door clears everything, and every module can come out. Share progress with Ara as captured views (perspective, front, side section) plus a short note, and put questions for him in a numbered list, one per line, each with a suggested answer; he replies from his phone.

Model in inches unless the builder says otherwise. Keep one layer per system (components, keep-outs, air, water, wiring, light, plants, enclosure) so each can be toggled. Make each component a named block and, where your tools allow, attach its spec as user text (part, size, voltage, controller pin) so parts can be swapped and a parts list pulled from the model. When a number you need isn't here, ask the builder. If you have to assume one, put "ASSUMED" in the object's name.

## The plant

*Cojoba angustifolia* ("Everfresh", monkey earring tree): a tropical legume with fern-like fronds whose leaflets open by day and fold at night. One tree with a single leader that leans toward the light, 16 in tall today. Ara hopes for about 28 in within a year. Design for at least that, plus the pot and the light clearance, with headroom if it's cheap. Pots: 2 gallons is the sweet spot (about a 10 in clay pot), with room for up to 4 (about 12 in across and 11 in tall). A wet 4 gallon clay pot with its tree weighs around 40 lb, so the plant floor and casters carry that per plant.

Ara grows it "hard": a short, warm, bright stress window each afternoon and a long recovery the rest of the day. The leaflets are the gauge. Dry air shows first as leaflets that won't open and tips that brown.

Inside the box it's kept around 76 to 80°F, reaches the 90s on summer afternoons, and sits at 70 to 95% relative humidity with mist several times an hour. Every inside surface lives at high humidity and will see condensation.

## How it works today

- **Air.** Slight negative pressure. Room air enters through a passive, screened intake low on one side, and an exhaust fan at the top pulls it out. A separate circulation fan stirs the inside air without exchanging it. Splitting mixing from exchange was the key architecture call of the first build.
- **Heat.** A ceramic heater turns on below 76°F and off above 78.5°F, and is forced off at 92°F.
- **Humidity.** An ultrasonic fogger in a water reservoir fires short bursts (20 to 30 s), up to a couple hundred a day. The controller steers by VPD (how drying the air is) on a daily schedule.
- **Cooling.** The only overheating is summer sun through a west window, roughly 4 to 8 PM. Mist is the main cooler. The exhaust helps only when the room is actually cooler than the canopy, and even then it pulses, because venting dries the air fast. It runs flat out at 99°F as a backstop.
- **Fresh air.** Once the box is sealed, a 2-minute exchange every 2 hours restores CO₂. The firmware has it ready; it's off while the old box leaks.
- **Light.** Not solved yet. See Grow lighting.
- **Watching.** Two temperature/humidity probes: one in the canopy (every decision uses it) and one outside at the intake (tells the controller whether venting would help). The controller logs every minute to the cloud and texts Ara on a stalled fan, overheating, a lost sensor, a dry reservoir or a dropped connection.

## Life support (carry forward, improve)

Any part can be swapped for a better one of the same kind without touching the firmware: 12 V 4-pin PWM fans, a 24 V on/off fogger, a 120 VAC heater switched by the relay, SHT31 probes. Two parts of the same kind can share one channel (two circ fans in parallel, a second fogger disc) if the switch and supply carry the current. A different kind of part (a 3-pin or AC fan, a motorized damper, another sensor) means a firmware change. That's allowed; flag it.

### Controller
- Particle Photon (the original Wi-Fi model) running Ara's firmware, about 1.5 × 0.8 in, powered by 5 V USB. Today it sits on a breadboard with plug-in MOSFET boards on top of the box. For the pro build Ara makes a proper board with screw terminals himself.
- It needs 2.4 GHz Wi-Fi, so no fully metal enclosure unless it uses its external-antenna jack (a one-line firmware change). Keep its USB port reachable, and its status light visible if that's easy.
- Pin map, fixed by the firmware:

| Pin | Drives |
|---|---|
| D0, D1 | canopy probe (data, clock) |
| D5, D6 | ambient probe (data, clock) |
| D2 | heater relay |
| D3 | fogger switch |
| A5, D4 | circ fan speed, circ fan power |
| WKP, A4 | exhaust fan speed, exhaust fan power |
| A0 to A3, DAC, D7, TX, RX | free for additions |

- The free pins are earmarked for a reservoir float switch, a door switch, and switching and dimming the grow light. Leave mounts and cable routes for all three; the firmware side is Ara's.

### Probes (2)
- SHT31 temperature/humidity sensors in sealed waterproof probes, four wires each (3.3 V, ground, data, clock). Both have the same fixed address, so each has its own wire pair to the controller. Keep each lead under about 6 ft and away from mains and fan wiring.
- **Canopy probe:** at leaf height in mixed, moving air, held clear of every leaf on a mount that adjusts as the tree grows. In July a leaf came to rest on it, which makes the probe read wet and the controller under-mist; leaf tips burned that month. Keep it out of the heater's line of sight, the mist plume and direct sun.
- **Ambient probe:** outside the box at the intake, in the air the exhaust will draw in. Away from the exhaust outlet, the electronics and direct sun.

### Heater
- Today: one 25 W ceramic heat emitter (the screw-in reptile kind: heat, no light) in a porcelain socket with a wire guard, 120 VAC, switched by a solid-state relay. Ara rates this kind the best heat source he's tried. It's far too small for a bigger box (see Heat budget).
- It dries whatever it can see. Twice in July the leaves nearest it dried out. At the wattage a bigger box needs, shroud it: a guarded housing the circ fan blows through, so the plants get warm air instead of radiant heat.
- In still air its heat pools at the top, and the canopy probe is the controller's only over-temperature guard, so the firmware keeps the circ fan turning whenever the heater is on. Put the heater in the circ fan's airflow. In v1 it sits right above the circ fan, which blows down.
- Its surface runs far hotter than acrylic tolerates (acrylic softens around 200°F), and water on a hot emitter can crack it. Give it real clearance or a metal shield from glazing, wood and leaves, keep it out of the mist plume and drip paths, use a fixture rated for its wattage, and use high-temperature wire near it.
- Working assumption until Ara says otherwise: two emitters of about 75 W each in one fan-blown housing, so one burning out doesn't leave the box cold overnight.

### Fogger and water supply
- Today: a 24 VDC ultrasonic fogger (a submerged transducer disc) in a 3 to 4 qt reservoir, about 350 ml of mist an hour while on. A fill lasted 2 to 3 weeks in summer; by late September the fogger ran about twice as much.
- Ara wants a better reservoir. The disc works best at a fixed depth, so a strong design is a small constant-level basin for the disc, fed from a bigger refill tank (2 gallons or so) through an inverted bottle or a float valve. The depth at the disc never changes, and a refill means topping up or swapping the tank from outside without opening the main door. At the rates seen so far, 2 gallons lasts about 2 to 5 weeks. Show the tank level from outside.
- In September the reservoir ran dry and the disc misted nothing for 14 hours. The firmware now texts on that. The hardware fix is a float switch in the basin on a free pin, so give it a mount.
- The mist should rise into the circ fan's airflow and spread through the box rather than hitting leaves, the canopy probe, the heater or anything electrical. Long bursts once drenched every surface, and the disc spits droplets around itself, so contain the splash.
- Basin and tank come out for cleaning without tools; scale and slime build up on the disc.

### Circulation fan
- 12 V 4-pin PWM PC fan, about 120 mm. Always on: about 10% speed by day, its slowest at night, up to 35% to prop humidity up, 100% while misting or venting. It mixes and never brings in room air. A wider build may need two on the same channel.
- Its bearing seized in August after two weeks of intermittent stalls. Choose a humidity-rated fan (sealed, fluid-dynamic or maglev bearing, or an IP-rated fan) and mount it so it swaps in minutes without tools.
- Its airflow has three jobs: sweep the heater so heat doesn't pool, carry the mist through the box, and keep the canopy probe in mixed air. Keep it gentle at the leaves. Sustained strong air dries them, which is why the firmware caps it at 35% outside misting.

### Exhaust fan
- 12 V 4-pin PWM PC fan, about 120 mm, at or near the top, far from the intake. It runs in bursts: 3 min on, 1 min off cooling pulses above 90°F when the room is cooler; humidity relief above 88%; the scheduled fresh-air exchange; a near-silent trickle on wet nights; 100% at 99°F. At night it's capped at 50% for noise.
- Mount it in a port cut to the fan body with a gasket. v1's hole was oversized and air looped back around the fan instead of leaving.
- Send its air straight into the room. It's close to saturated and shouldn't cross the electronics.
- When it's off, a low intake plus a high exhaust opening make a chimney: warm, wet air rises out and room air flows in, all winter. Add a backdraft flap light enough that the fan opens it at its slowest speed. (Sealing the bottom of v1 in June was the biggest humidity fix so far, and it probably worked by cutting off this same chimney.)

### Intake
- A passive, screened port low on one side, far from the exhaust, so incoming air sweeps the whole box. Make it at least as open as the exhaust fan so it doesn't choke it. The ambient probe sits just outside.

### Power and switching (in the dry bay)
- One power cord leaves the cabinet, from a fused, switched inlet (an IEC inlet with built-in fuse and switch is the clean way), on a GFCI-protected outlet or with a GFCI in line. Water and 120 VAC share this cabinet.
- Inside: a 12 V supply (fans), a 24 V supply (fogger), 5 V USB (controller) and the light driver, with all DC grounds tied together.
- Heater on a solid-state relay (3 to 32 VDC control, 120 VAC load) with a heat sink. Fogger, circ fan power and exhaust fan power on logic-level MOSFETs, with flyback diodes across the fans. The fans' tach wires are cut on purpose; connected, they leak current and keep a switched-off fan idling.
- An independent thermal cutoff wired in series with the heater, so a stuck relay or a misplaced probe can't cook the box. At the wattage this build needs, it isn't optional.

## Grow lighting

Not solved in v1, and the biggest open system. Today an LED bulb in a clamp reflector hangs from the top on a plug-in timer (about 6 AM to 8 PM). The plant barely responds to it; it runs on window light.

The new light isn't chosen, and Ara has parked that decision, so design for a range rather than a model. At one end is a slim white panel overhead. At the other are Ara's own Verdical grow bars, from the grow-tower company he founded: 4 to 6 ft long, so in this cabinet they would stand upright (about 48 in fits the humid zone) instead of lying across the top, and tuned for growth rather than looks. Keep both possible: a top mount and upright channels or rails (the corners are natural spots), driver space and cable paths for either, and a light that swaps without rework.

- **Target.** About 300 PPFD at the top of the canopy across the whole planted area, dimmable from roughly 150 to 500, on a 14-hour day (about 15 mol/m² of light a day).
- **Color.** It's a showpiece, and white full-spectrum light (around 3500 K, good color rendering) shows the tree best. Growth-tuned light often reads pink; if that's the pick, hiding the emitters matters more.
- **Size.** Roughly 15 to 20 W of a good LED board per square foot of planted area. A 24 × 18 in footprint needs 45 to 60 W, which is why the 100 W panels shortlisted in September (ViparSpectra P1000, Mars Hydro TS600) fit at about half power with headroom. Slim LED bars work too if they add up.
- **If it's a new light.** White full-spectrum, an efficient board (about 2.5 µmol/J or better), sized to the planted area, and an external driver the controller can dim.
- **Coverage.** The light's footprint matches the planted area. Side-by-side plantings need the light to reach every plant. Cuttings want less (100 to 200 PPFD), so a nook can sit where the light falls off.
- **Height.** The canopy climbs from 16 to 28 in. Leave about 12 in or more between the light and the canopy at full height, or make the light height-adjustable; dimming covers the rest.
- **Wet or dry mount.** Either a humidity-rated fixture (IP65 or better) inside, or the LED in the dry bay shining down through a sealed glass ceiling. The ceiling option keeps it dry and serviceable from above, costs about a tenth of the light, and beads with condensation underneath, so slope it or plan to wipe it. The driver always lives in the dry bay.
- **Glare.** Hide the emitter from eye level (a shallow valance or a recessed mount) so the cabinet glows like daylight and nobody looks straight into the diodes.
- **Heat.** An LED inside the box turns most of its watts into heat there. That's welcome on winter days. On summer afternoons the sun does the lighting.
- **Control.** The controller will own the light: a switched feed and a dimming line (PWM or 0 to 10 V, depending on the light) from free pins, which Ara wires. Then the firmware can run the day length, dim while the afternoon sun is on the box (it already detects that), and log light hours. A timer works until then.

## Cable management

Also not solved in v1, where cables run loose and taped. Design the harness as part of the furniture.

- **Three classes, three routes.** Mains (heater, light driver, supplies), low-voltage power (12 V fans, 24 V fogger, LED DC) and signal (probes, fan speed lines, switches) each get their own path. The probe lines are the touchy ones; keep them away from mains and the fan PWM wires.
- **Built-in chases.** Run cables inside a frame member or a spine with a removable cover, so none show and each can be reached.
- **One bulkhead.** Every wet-zone device ends in a short pigtail with a keyed, locking connector at one patch panel between the humid zone and the dry bay. Use a different connector for each class so a 24 V plug can't land in a 12 V socket. Any module then comes out by unplugging one connector. Working assumption until Ara says otherwise: GX16 aviation connectors (about 16 mm panel holes, screw-locking, keyed by pin count) for low voltage and sensors, plus a panel-mount AC connector for the heater.
- **Electronics tray.** Controller, switching, supplies and driver live on one tray that slides or lifts out with a single harness connector. Ara builds the tray; reserve about 14 × 10 × 4 in for it (working assumption), with airflow and easy access.
- **Service loops.** Leave slack wherever something moves: the canopy probe's height mount, an adjustable light, the basin that lifts out. Mount nothing electrical on the door.
- **Seals and heat.** Sealed cable glands with drip loops wherever a cable crosses into the humid zone; no taped holes. High-temperature wire near the heater, and no other cable in its keep-out.
- **Labels.** Label both ends of every cable and color-code by class. Put the harness on the wiring layer of the model so it can be built from it.

## Water and the floor

- No standing water by design. The floor still has to take water: spills when watering or refilling, condensation running off the glass, and air that sits at 70 to 95% humidity all the time.
- Make it a sealed pan sloped to one low corner that empties through a drain or a pull-out tray. Wipeable, no exposed end grain, no seams that hold water.
- Single glazing will run with condensation on cold nights. Every glazing joint and bottom rail sheds water into the pan, never into the frame or onto the room's floor.
- Nothing inside can rot, swell or grow mold at 95% humidity. Keep inside corners smooth and cleanable.

## Heat budget

Today's 25 W heater has run at 85 to 100% since mid-September and still holds the canopy only about 4°F above a 68 to 70°F room, short of the 76°F target. Single glass or acrylic loses heat about as fast as v1's plastic sheeting, so v1's own numbers size the new heater, and textbook figures agree: holding 76°F in a 62°F winter room takes about 4 W of steady heat per square foot of glazing.

| Build | Glazing | Steady heat at the coldest |
|---|---|---|
| v1 tent today, 13.5 × 17.5 × 35 in | about 18 sq ft | about 80 W (it has 25) |
| All-glass box, 24 × 18 × 52 in | about 36 sq ft | about 150 W |
| Same box, opaque insulated back, top and base | about 22 sq ft | about 100 W |

Install about a third more than the steady figure so the heater isn't pinned at full power. Wider builds scale with their glass. Sealing well (the backdraft flap included) and insulating every opaque panel are the cheap savings. The light adds heat by day; the heater carries the night alone.

## Layout options

v1's interior is 13.5 × 17.5 × 35 in and outgrown. A single-tree build lands around 24 × 18 in of floor and 52 in of clear height for the pot, the tree and the light gap, plus the dry bay and the water zone; wider builds grow from there.

Three zones hold in any layout. The **humid zone** holds the plants, canopy probe, heater, circ fan, fogger basin and the light (or its glass ceiling). The **dry bay** holds the electronics tray, supplies, light driver and power inlet, sealed from the humid zone, vented to the room, and above or beside the water, never below it. **Water** sits at the bottom: basin, tank and floor pan.

- **Polished single-tree stack.** Electronics on top, light under the top, tree in the middle, water at the bottom. Ara's September concept (the "Vitrine") worked this out as a wood and glass cabinet; one fix to it: it put the exhaust fan inside the electronics bay, which would push wet air across the electronics.
- **Wider, side by side.** One canopy probe runs the whole box, so every plant shares one climate and the probe should sit where it reads the planted area as a whole (a second canopy probe later is firmware work). Wider also means more glass to heat, more light to cover the footprint, and circ airflow that reaches every corner.
- **Cuttings nook.** Cuttings want it wetter, stiller and dimmer than the tree. A lidded clear box inside the chamber gets there by borrowing the chamber's warmth and humidity, with no second control zone.

## Setup, collapse and daily care

**Setup and collapse.** Once installed it will likely stand for years, so rigidity and lasting seals come first. A move still can't be a blocker: the cabinet should come apart in an afternoon with hand tools into pieces that fit in a car and one person can carry, and go back together with its seals intact. Favor bolted or cam-locked frame joints and threaded inserts over glue and wood screws, and glazing held in removable stops with gaskets over glazing glued in for good. Modules lift out whole: electronics tray, light, water module (basin, tank and fogger), heater and circ fan module, probes. A full-height glass side at about 24 × 52 in weighs roughly 15 to 30 lb depending on thickness (tempered for safety); acrylic weighs less than half that and scratches more easily. Casters, two locking, plus levelers so the floor still drains on an uneven floor.

**Daily care.** Every routine job should be quick, and most shouldn't need the main door.

| Task | How often | Target |
|---|---|---|
| Check plants and readings | daily | a look through the glass, door shut |
| Water the plants | as they need | pot reachable without moving anything |
| Refill water | every few weeks | from outside, level visible |
| Clean basin and fogger disc | every refill or two | lifts out, no tools |
| Empty and wipe the floor pan | as needed | drain or pull-out tray |
| Wipe the glass inside | as needed | every inside surface reachable |
| Raise the canopy probe | as the tree grows | slide and lock, no tools |
| Swap a fan, heater or probe | when one fails | minutes, unplug at the bulkhead |
| Firmware update | as needed | over Wi-Fi; USB reachable for recovery |

A door switch on a free pin would let the controller pause mist and fans while the door is open, so reaching in doesn't mean a face full of fog.

## Sealing, placement and noise

- **Sealing** is the biggest humidity lever. In June, closing the gap at the bottom of v1 flattened a 65 to 85% humidity swing into a steady 77%, more than any software change did. The door will be the biggest leak, so give it a continuous gasket and a latch that pulls it tight. Seal every fixed panel; no open seams.
- **Door.** Reach every plant through one big door. Ara's September preference was a single full-height front door, hinged left, handle right.
- **Placement.** It lives near a west window and may move. Summer afternoons put about 2 hours of direct sun on it. The light is wanted; the heat is the problem. Make it easy to shade the sun-facing side (a removable screen or a slot for one).
- **Noise.** It shares a lived-in room, and fan noise is a real cost. Isolate the fans from glazing and frame so nothing hums.

## Ara's preferences (open to discussion)

- Single-glazed glass or acrylic. September's concept had glass on all four sides; Ara is open to other layouts now, so an opaque, insulated back that also bounces light onto the plants is fair game.
- A showpiece that reads as furniture. The September concept went warm and tropical (honey-toned wood, brass hardware); the builder's eye is welcome.
- Budget targets: about $250 for finished panel material and $75 for the frame. Hardware goes on its own list for Ara to approve. Components are separate.

## The builder's call

Layout, frame material and structure, joinery, glazing material and thickness, door and latch hardware, how it knocks down, and the look.

[Builder: add your shop setup, the materials you like working, and any limits here.]

## Still to fill in

- Exact sizes and models of today's fans and fogger (measure them).
- The grow light (Ara's call, parked; design for the range in Grow lighting).
- The hardware list and its prices, for Ara to approve.
- The working assumptions above (heater pair, GX16 connectors, tray size) stand until Ara changes them.
