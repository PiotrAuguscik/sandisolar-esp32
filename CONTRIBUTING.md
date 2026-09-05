# Contributing

Run the host tests and compile the affected ESP example before submitting a
change; commands and toolchain versions are in [README](README.md).

Keep protocol validation, profile interpretation, BLE lifecycle and application
policy separate. Unknown values remain raw or explicitly experimental. Include
tests for rejection/failure paths and preserve existing numeric API IDs.

Parameter confirmations should identify the marketed model, firmware version,
register/table, proposed scale and a corresponding display/app value. Do not
publish serial numbers, logger PNs, site information, credentials, complete
phone bug reports, private captures or decompiled proprietary code. Use
synthetic/minimized fixtures for public regression tests.

Research tools and field-laboratory firmware belong outside this library. The
runtime, builds and tests must work without sibling research directories.

Do not add undocumented register writes or inverter-control experiments.
Control support requires a separate safety and recovery design.
