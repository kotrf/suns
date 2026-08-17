# Ship design foundation

Suns! should eventually make ship roles emerge from fitted hardware rather than from fixed classes. The first implementation keeps the current Qt presentation compatible, but the simulation now has explicit ship designs and components.

## Current model

A `Fleet` references a `ShipDesign`. A design has a hull mass, hull build cost and a list of installed components. The initial component catalog is deliberately tiny:

- **Fusion Drive** — adds mass, build cost and engine thrust;
- **Long Range Scanner** — adds mass, build cost and survey range;
- **Colony Module** — adds mass, build cost and enables colonization.

The two starting designs reproduce the current game behaviour through the same component rules:

- **Scout** = light hull + Fusion Drive + Long Range Scanner;
- **Colony Ship** = heavier hull + Fusion Drive + Colony Module.

## Derived properties

Total mass is hull mass plus component masses. Build cost is hull cost plus component costs.

Travel speed is derived from engine thrust and fitted mass:

`speed = total_engine_thrust * 10 / total_mass`

The constants are tuning placeholders, but the relationship is intentional. Adding useful equipment makes a ship heavier, so a designer must trade capability against speed unless a stronger engine is fitted.

Survey range is derived from installed scanner components. Colonization is permitted only when the fitted design contains a component that enables it.

The default designs are tuned so the existing gameplay remains stable:

- Scout: mass 59.5, speed 100, survey range 90, build cost 8;
- Colony Ship: mass 85, speed 70, no survey scanner, build cost 12.

A test hybrid made by adding the Colony Module to the Scout design keeps its scanner and gains colonization capability, but becomes slower and more expensive. This is the kind of meaningful fit tradeoff the final ship designer should produce.

## Temporary compatibility layer

`FleetRole` still exists for the current Qt marker shape/colour and old UI helpers. It is not authoritative for turn resolution. Movement speed, sensor sweep and colonization permission are derived from `ShipDesign`.

A later UI-focused step should remove this compatibility role and derive presentation from the fitted design as well.

## Future direction

The component system is intentionally extensible. Candidate future equipment includes fuel tanks, stronger engines, armor, shields, beam and missile weapons, point defense, cargo, minelayers, mining equipment, jammers, cloaking and specialized sensors.

Those components should not be added merely to create a long catalog. Each should introduce a strategic design decision through mass, cost, power, range, survivability, signature, logistics or another interacting constraint.

The eventual designer should let the player save named designs and build those designs at colonies. Technology should unlock hulls and components rather than simply replacing ships with fixed higher-level classes.
