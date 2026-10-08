# ft/s Material Extrusion — Calibration To-Do

What you need to **do** and the information you need to **collect from the
installed system** to make the feet-per-second (ft/s) extrusion reading
accurate. The firmware stub is already in place
(`SAMD21_Project/src/services/config.c` → `config_calc_feet_per_rev_milli()`);
this doc is the checklist for filling it in and calibrating it.

---

## Background (why this is needed)

The encoder measures **encoder-shaft revolutions**. We want **feet of metal per
shaft revolution** (`feet_per_rev`), from which the firmware already computes
`ft_s = rpm / 60 × feet_per_rev`.

The physical chain is usually:

```
encoder shaft --(gearing)--> measuring roller/sprocket --> linear metal
```

The geometry gives a **theoretical estimate**. Chain slip and unknown
downstream geometry mean the estimate is only a starting point — the accurate
number comes from **calibrating on the real machine** (see CLAUDE.md §6
decisions log: "FEET_PER_REV is calibrated on the machine").

---

## Information to collect from the system

Fill these into the `CFG_GEOM_*` defines in `config.c`:

- [ ] **Encoder mounting** — is the encoder mounted directly on the measuring
      roller/sprocket shaft, or upstream through a gearbox/chain?
      → decides whether there is a gear ratio at all.

- [ ] **Gear ratio** (only if NOT direct-mounted): the teeth counts between the
      encoder shaft (driver) and the measuring element (driven).
      - `CFG_GEOM_DRIVER_TEETH`  = encoder-shaft gear teeth
      - `CFG_GEOM_DRIVEN_TEETH`  = measuring-element gear teeth
      - If direct-mounted (no gearing): set **both to 1**.
      - Give them as integer teeth counts (keeps the math exact).

- [ ] **Measuring element effective circumference** → `CFG_GEOM_CIRCUM_MILLI_IN`
      (in **milli-inches**, i.e. thousandths of an inch). Pick whichever is
      more accurate for the hardware:
      - **Measuring wheel/roller:** circumference = π × diameter. Measure the
        circumference directly with a tape if you can — more accurate than
        measuring the diameter and multiplying.
      - **Sprocket + chain:** chain pitch × number of teeth = pitch
        circumference. More accurate than the physical sprocket diameter.
      - Convert your measurement to milli-inches before entering it
        (e.g. 6.000 in → 6000; if you measured in mm, divide mm by 25.4 to get
        inches first).

- [ ] **Units you measured in** (inches or mm) — note it so the conversion to
      milli-inches is done correctly.

---

## Steps to do (in order)

1. [ ] **Collect the data above** from the installed machine.

2. [ ] **Enter the geometry** into the `CFG_GEOM_*` defines in
       `SAMD21_Project/src/services/config.c`.

3. [ ] **Switch the accessor on:** change `config_feet_per_rev_milli()` to
       `return config_calc_feet_per_rev_milli();` (instead of the flat
       `CFG_FEET_PER_REV_MILLI` placeholder). Rebuild.

4. [ ] **Sanity-check the estimate:** at a known RPM, does the ft/s on the LCD /
       telemetry look physically plausible? This catches unit mistakes (a 12×
       or 25.4× error jumps out immediately).

5. [ ] **Calibrate on the machine** (this is what makes it accurate):
       - Run a **known length** of material through (e.g. mark and measure a
         10 ft length as it passes).
       - Compare the **actual length** to the length the node computed
         (counted revs × current `feet_per_rev`).
       - Compute the trim: `correction = actual_length / computed_length`.
       - Enter it as parts-per-thousand into `CFG_GEOM_CORRECTION_PPT`
         (1000 = no correction; e.g. actual/computed = 0.98 → 980).
       - Rebuild and re-run the known-length test to confirm it now matches.

6. [ ] **Record the final numbers** (geometry + correction) somewhere durable —
       they belong in NVM once P5 lands, and should be noted per physical node
       (each machine's geometry may differ).

---

## Notes / gotchas

- All firmware math is **integer** (no `%f` in XC32). The stub holds everything
  as integers and uses `uint64_t` internally so the multiplies don't overflow.
- `feet_per_rev` is stored as **milli-feet** (×1000) everywhere.
- A zero `CFG_GEOM_DRIVEN_TEETH` is treated as a config error and the stub falls
  back to the placeholder (avoids divide-by-zero).
- Per-node: different machines may have different rollers/gearing — don't assume
  one calibration transfers to another node.
