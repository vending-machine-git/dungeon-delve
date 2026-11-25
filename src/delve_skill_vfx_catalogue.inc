design("Bloodrage", Crimson, Ember, 420, {
    cast(S::Fang,A::Hands,4,M::Strike), cast(S::Blade,A::Hands,5,M::Strike,1),
    active(S::Fang,A::Hands,2.8,G::Strength,M::Pulse), active(S::Steps,A::Feet,6,G::Speed,M::Flow,1)}),
design("War Cry", Ember, Ivory, 687, {
    cast(S::Sound,A::Hands,7,M::Expand), cast(S::Wave,A::Feet,-1,M::Expand,1),
    hit(S::Fang,5,M::Expand), control(S::Fang,3,M::Pulse)}),
design("Cleave", Ivory, Crimson, 24, {
    cast(S::Crescent,A::Hands,17,M::Sweep), cast(S::Blade,A::Hands,13,M::Sweep,1,0,4),
    hit(S::Crosscut,5), hit(S::Spark,4,M::Shatter,1)}),
design("Second Wind", Jade, Ivory, 168, {
    cast(S::Breath,A::Hands,7,M::Expand,1), cast(S::Heart,A::Hands,4,M::Rise), cast(S::Helix,A::Body,6,M::Rise,0,0,5)}),
design("Shield Advance", Azure, Ivory, 44, {
    cast(S::Shield,A::Hands,6,M::Strike), cast(S::Steps,A::Feet,9,M::Flow,1),
    active(S::Shield,A::Hands,4,G::Resistance,M::Hold), active(S::Wind,A::Feet,6,G::Speed,M::Flow,1)}),
design("Sweeping Strike", Ivory, Gold, 25, {
    cast(S::Blade,A::Hands,18,M::Sweep), cast(S::Crescent,A::Hands,20,M::Sweep,1,0,5), hit(S::Blade,6,M::Sweep)}),
design("Iron Resolve", Jade, Ivory, 168, {
    cast(S::Shield,A::Hands,5,M::Strike,1), cast(S::Cross,A::Hands,3.5,M::Rise),
    active(S::Cross,A::Hands,2.2,G::Regeneration,M::Rise), active(S::Helix,A::Body,4,G::Regeneration,M::Rise,1)}),
design("Challenge", Gold, Crimson, 687, {
    cast(S::Banner,A::Hands,6,M::Strike), cast(S::Sound,A::Hands,10,M::Expand,1),
    link(S::Spear,M::Strike), hit(S::Shield,4,M::Strike), control(S::Steps,4,M::Contract,1)}),
design("Emergency Mend", Jade, Ivory, 168, {
    cast(S::Cross,A::Hands,6,M::Strike,1), cast(S::Heart,A::Hands,4.5,M::Pulse), cast(S::Helix,A::Body,8,M::Rise,0,0,3)}),
design("Purifying Breath", Ivory, Jade, 165, {
    cast(S::Breath,A::Hands,9,M::Expand), cast(S::Spark,A::Body,6,M::Shatter,1),
    active(S::Feather,A::Hands,3,G::Resistance,M::Rise)}),
design("Life Ward", Jade, Gold, 251, {
    cast(S::Shield,A::Hands,6,M::Reveal,1), cast(S::Heart,A::Hands,3.5,M::Pulse),
    active(S::Shield,A::Hands,4,G::Resistance,M::Hold,1), active(S::Heart,A::Hands,2,G::Regeneration,M::Pulse)}),
design("Merciful Rebuke", Gold, Jade, 169, {
    cast(S::Cross,A::Hands,4,M::Strike), link(S::Lance,M::Strike), hit(S::Cross,5,M::Expand), control(S::Helix,3,M::Contract,1)}),
design("Vanish", Smoke, Violet, 166, {
    cast(S::Mask,A::Hands,5,M::Contract,1), cast(S::Smoke,A::Body,7,M::Expand),
    active(S::Mask,A::Hands,2.3,G::Hidden,M::Hold,1)}),
design("Dirty Cut", Crimson, Ivory, 26, {
    cast(S::Blade,A::Hands,10,M::Sweep), cast(S::Crosscut,A::Hands,5,M::Strike,1),
    hit(S::Blade,5,M::Sweep), control(S::Crosscut,3,M::Contract)}),
design("Quickstep", Ivory, Smoke, 756, {
    cast(S::Steps,A::Feet,8,M::Flow), cast(S::Spark,A::Feet,5,M::Shatter,1),
    active(S::Steps,A::Feet,5,G::Speed,M::Flow)}),
design("Pocket Sand", Ember, Gold, 757, {
    cast(S::Pellets,A::Hands,18,M::Expand), cast(S::Smoke,A::Hands,5,M::Expand,1),
    hit(S::Eye,4,M::Contract,1), hit(S::Pellets,5), control(S::Confusion,3,M::Spin)}),
design("Trailblaze", Jade, Gold, 752, {
    cast(S::Compass,A::Feet,9,M::Expand,1), cast(S::Leaf,A::Hands,4,M::Flow),
    active(S::Steps,A::Feet,7,G::Speed,M::Flow), active(S::Leaf,A::Feet,3,G::Speed,M::Flow,1)}),
design("Camp Remedy", Jade, Azure, 52, {
    cast(S::Vial,A::Hands,4,M::Reveal), cast(S::Leaf,A::Hands,5,M::Rise), cast(S::Well,A::Body,6,M::Contract,1,0,5)}),
design("Hunter's Sweep", Jade, Ivory, 24, {
    cast(S::Crescent,A::Hands,20,M::Sweep,1), cast(S::Leaf,A::Hands,7,M::Sweep), hit(S::Crescent,6,M::Sweep,1)}),
design("Read the Wild", Jade, Gold, 753, {
    cast(S::Compass,A::Feet,12,M::Spin,1), cast(S::Eye,A::Hands,5,M::Reveal),
    active(S::Eye,A::Hands,2.5,G::Sight,M::Pulse), active(S::Leaf,A::Hands,3,G::Resistance,M::Hold,1)}),
design("Lay on Hands", Gold, Jade, 168, {
    cast(S::Cross,A::Hands,5,M::Reveal), cast(S::Wing,A::Hands,6,M::Expand), cast(S::Heart,A::Body,5,M::Rise,1,0,6)}),
design("Hallowed Guard", Gold, Ivory, 165, {
    cast(S::Shield,A::Hands,7,M::Reveal), cast(S::Cross,A::Hands,3,M::Strike,1),
    active(S::Shield,A::Hands,4,G::Resistance,M::Hold), active(S::Cross,A::Hands,2,G::Regeneration,M::Rise,1)}),
design("Judgment", Gold, Ivory, 165, {
    cast(S::Hammer,A::Hands,6,M::Strike), link(S::Lance,M::Strike,1,.3), hit(S::Sun,7,M::Expand), hit(S::Hammer,5,M::Fall,1)}),
design("Pilgrim's March", Gold, Ivory, 178, {
    cast(S::Wing,A::Feet,7,M::Rise), cast(S::Steps,A::Feet,9,M::Flow),
    active(S::Steps,A::Feet,6,G::Speed,M::Flow), active(S::Feather,A::Feet,3,G::Levitation,M::Rise,1)}),
design("Hostile Negotiation", Gold, Violet, 242, {
    cast(S::Coin,A::Hands,4,M::Spin), cast(S::Sound,A::Hands,9,M::Expand,1),
    hit(S::Coin,5), control(S::Confusion,3,M::Spin,1)}),
design("Caravan Sprint", Gold, Ember, 756, {
    cast(S::Banner,A::Hands,4,M::Flow), cast(S::Steps,A::Feet,10,M::Flow,1),
    active(S::Steps,A::Feet,6,G::Speed,M::Flow), active(S::Coin,A::Feet,2,G::Speed,M::Spin,1)}),
design("Insurance Policy", Gold, Jade, 83, {
    cast(S::Card,A::Hands,5,M::Reveal), cast(S::Heart,A::Hands,3,M::Rise,1),
    active(S::Shield,A::Hands,4,G::Resistance,M::Hold)}),
design("Flash Buyout", Gold, Violet, 38, {
    cast(S::Coin,A::Hands,5,M::Contract), cast(S::Eye,A::Hands,4,M::Reveal,1),
    active(S::Coin,A::Hands,2,G::Hidden,M::Spin), active(S::Eye,A::Hands,2.5,G::Sight,M::Pulse,1)}),
design("Arcane Lance", Violet, Ivory, 169, {
    cast(S::Rune,A::Hands,5,M::Contract), cast(S::Lance,A::Hands,14,M::Strike,1),
    link(S::Lance,M::Strike,0,.32), hit(S::Mirror,5,M::Shatter,1)}),
design("Frost Ring", Azure, Ivory, 172, {
    cast(S::Snowflake,A::Feet,8,M::Reveal,1), cast(S::Wave,A::Feet,-1,M::Expand),
    hit(S::Icicle,5,M::Rise,1), control(S::Snowflake,3.5,M::Hold)}),
design("Mirror Ward", Azure, Ivory, 175, {
    cast(S::Mirror,A::Hands,7,M::Reveal), cast(S::Hexagon,A::Body,8,M::Spin,1),
    active(S::Mirror,A::Hands,4,G::Reflection,M::Pulse), active(S::Mirror,A::Body,6,G::Reflection,M::Spin,1)}),
design("Ley Siphon", Azure, Violet, 170, {
    cast(S::Rune,A::Feet,10,M::Contract,1), cast(S::Well,A::Hands,7,M::Contract), cast(S::Helix,A::Body,6,M::Rise,0,0,5)}),
design("Runic Bolt", Violet, Azure, 169, {
    cast(S::Rune,A::Hands,5,M::Spin), cast(S::Hexagon,A::Hands,3,M::Contract,1),
    link(S::Lightning,M::Strike), hit(S::Rune,5,M::Expand,1)}),
design("Spellguard", Azure, Violet, 251, {
    cast(S::Hexagon,A::Hands,7,M::Reveal), cast(S::Wind,A::Feet,8,M::Flow,1),
    active(S::Hexagon,A::Hands,4,G::Resistance,M::Hold), active(S::Wind,A::Feet,5,G::Speed,M::Flow,1)}),
design("Charged Break", Violet, Azure, 171, {
    cast(S::Rune,A::Hands,6,M::Contract), cast(S::Blast,A::Hands,10,M::Expand,1,0,5),
    link(S::Lightning), hit(S::Web,6,M::Expand), control(S::Web,4,M::Hold,1)}),
design("Battle Recharge", Azure, Ivory, 420, {
    cast(S::Well,A::Hands,6,M::Contract), cast(S::Rune,A::Feet,8,M::Spin,1),
    active(S::Rune,A::Hands,3,G::Amplification,M::Spin), active(S::Spark,A::Hands,4,G::Amplification,M::Pulse,1)}),
design("Vanishing Act", Violet, Gold, 166, {
    cast(S::Card,A::Hands,5,M::Shatter,1), cast(S::Smoke,A::Body,7,M::Contract),
    active(S::Card,A::Hands,2.5,G::Hidden,M::Spin), active(S::Wind,A::Feet,5,G::Speed,M::Flow,1)}),
design("Punchline", Gold, Violet, 174, {
    cast(S::Mask,A::Hands,5,M::Strike), cast(S::Sound,A::Hands,12,M::Expand,1),
    hit(S::Mask,5,M::Shatter), control(S::Confusion,4,M::Spin,1)}),
design("Juggler's Flurry", Gold, Violet, 239, {
    cast(S::Knives,A::Hands,17,M::Expand), cast(S::Card,A::Hands,4,M::Spin,1),
    link(S::Pellets,M::Strike), hit(S::Card,4,M::Shatter,1)}),
design("Encore", Jade, Azure, 251, {
    cast(S::Mask,A::Hands,5,M::Reveal,2), cast(S::Heart,A::Hands,4,M::Rise), cast(S::Well,A::Body,6,M::Contract,1,0,5)}),
design("Grave Silence", Smoke, Violet, 683, {
    cast(S::Tomb,A::Hands,6,M::Rise), cast(S::Sound,A::Hands,12,M::Contract,1),
    hit(S::Sleep,4,M::Rise,1), control(S::Sleep,3,M::Rise,1)}),
design("Ashen Ward", Smoke, Ivory, 166, {
    cast(S::Tomb,A::Hands,6,M::Reveal), cast(S::Smoke,A::Body,8,M::Rise),
    active(S::Tomb,A::Hands,3.5,G::Resistance,M::Hold,1), active(S::Smoke,A::Hands,3,G::Hidden,M::Rise)}),
design("Gravestone Blow", Smoke, Ivory, 66, {
    cast(S::Tomb,A::Hands,7,M::Fall), cast(S::Hammer,A::Hands,11,M::Strike,1,0,4),
    hit(S::Blast,6), control(S::Tomb,3,M::Hold)}),
design("Wake the Dead", Violet, Azure, 174, {
    cast(S::Skull,A::Hands,5,M::Rise), cast(S::Well,A::Feet,9,M::Contract,1),
    active(S::Eye,A::Hands,3,G::Sight,M::Pulse), active(S::Skull,A::Body,3,G::Sight,M::Rise,1)}),
design("Smoke Veil", Smoke, Ivory, 166, {
    cast(S::Smoke,A::Feet,9,M::Expand), cast(S::Shuriken,A::Hands,4,M::Contract,1),
    active(S::Smoke,A::Hands,2,G::Hidden,M::Flow)}),
design("Shuriken Fan", Ivory, Smoke, 239, {
    cast(S::Shuriken,A::Hands,5,M::Spin), cast(S::Knives,A::Hands,22,M::Expand),
    link(S::Knives,M::Strike), hit(S::Shuriken,4,M::Spin)}),
design("Nerve Strike", Ivory, Azure, 24, {
    cast(S::Spear,A::Hands,12,M::Strike), cast(S::Spark,A::Hands,4,M::Strike,1),
    hit(S::Crosscut,4,M::Strike), control(S::Lightning,4,M::Pulse,1)}),
design("Wind Step", Ivory, Azure, 178, {
    cast(S::Feather,A::Hands,5,M::Rise), cast(S::Wind,A::Feet,10,M::Flow),
    active(S::Wind,A::Feet,6,G::Speed,M::Flow), active(S::Wing,A::Feet,3,G::Levitation,M::Rise,1)}),
design("Palm Shock", Gold, Ivory, 23, {
    cast(S::Cross,A::Hands,4,M::Strike,1), cast(S::Breath,A::Hands,13,M::Expand),
    hit(S::Wave,5,M::Expand), control(S::Wave,3,M::Contract)}),
design("Centering Breath", Jade, Azure, 168, {
    cast(S::Breath,A::Hands,7,M::Contract,2), cast(S::Heart,A::Hands,4,M::Pulse), cast(S::Helix,A::Body,6,M::Rise,1,0,6)}),
design("Wind Walk", Ivory, Jade, 756, {
    cast(S::Wing,A::Hands,7,M::Expand), cast(S::Wind,A::Feet,11,M::Flow),
    active(S::Steps,A::Feet,5,G::Speed,M::Flow,1), active(S::Wing,A::Feet,4,G::Levitation,M::Rise)}),
design("Diamond Skin", Azure, Ivory, 44, {
    cast(S::Mirror,A::Hands,7,M::Reveal,1), cast(S::Mirror,A::Body,10,M::Contract),
    active(S::Mirror,A::Hands,3,G::Resistance,M::Hold), active(S::Hexagon,A::Body,5,G::Resistance,M::Spin,1)}),
design("Binding Pulse", Violet, Ivory, 174, {
    cast(S::Web,A::Hands,8,M::Expand), cast(S::Rune,A::Hands,5,M::Contract,1),
    hit(S::Web,6,M::Contract), control(S::Web,4.5,M::Hold,1)}),
design("Master's Aegis", Violet, Jade, 165, {
    cast(S::Rune,A::Feet,9,M::Spin), cast(S::Shield,A::Hands,6,M::Reveal),
    active(S::Shield,A::Hands,4,G::Resistance,M::Hold), active(S::Helix,A::Body,4,G::Regeneration,M::Rise,1)}),
design("Soul Exchange", Azure, Violet, 174, {
    cast(S::Skull,A::Hands,5,M::Contract,1), cast(S::Well,A::Hands,7,M::Rise), cast(S::Rune,A::Feet,9,M::Contract,1,0,5)}),
design("Spectral Survey", Violet, Azure, 166, {
    cast(S::Eye,A::Hands,5,M::Reveal,1), cast(S::Skull,A::Body,5,M::Shatter),
    active(S::Eye,A::Hands,3,G::Sight,M::Pulse), active(S::Skull,A::Hands,2,G::Hidden,M::Rise,1)}),
design("Blood Price", Crimson, Smoke, 420, {
    cast(S::Fang,A::Hands,5,M::Contract), cast(S::Skull,A::Body,6,M::Rise,1),
    active(S::Fang,A::Hands,3,G::Strength,M::Pulse), active(S::Wind,A::Feet,5,G::Speed,M::Flow,1)}),
design("Crimson Drain", Crimson, Violet, 174, {
    cast(S::Fang,A::Hands,5,M::Contract), link(S::Siphon,M::Contract,0,.3),
    hit(S::Heart,4,M::Contract), hit(S::Fang,5,M::Shatter,1)}),
design("Night Shroud", Smoke, Crimson, 166, {
    cast(S::Wing,A::Hands,7,M::Contract), cast(S::Smoke,A::Body,8,M::Rise),
    active(S::Wing,A::Hands,3,G::Hidden,M::Hold,1), active(S::Shield,A::Hands,3,G::Resistance,M::Hold)}),
design("Dread Pulse", Crimson, Smoke, 687, {
    cast(S::Skull,A::Hands,6,M::Strike,1), cast(S::Wave,A::Feet,-1,M::Expand),
    hit(S::Fang,5,M::Expand), control(S::Skull,3.5,M::Pulse,1)}),
design("Mind Fracture", Violet, Azure, 174, {
    cast(S::Eye,A::Hands,5,M::Contract), link(S::Lightning,M::Strike),
    hit(S::Mirror,6,M::Shatter,1), control(S::Confusion,4,M::Spin)}),
design("False Opening", Violet, Ivory, 166, {
    cast(S::Mirror,A::Hands,6,M::Shatter,1), cast(S::Mask,A::Body,6,M::Contract),
    active(S::Mirror,A::Hands,3,G::Hidden,M::Spin), active(S::Steps,A::Feet,5,G::Speed,M::Flow,1)}),
design("Cold Snap", Azure, Ivory, 172, {
    cast(S::Snowflake,A::Hands,5,M::Strike,1), cast(S::Icicle,A::Hands,10,M::Expand),
    link(S::Lightning,M::Strike,1), hit(S::Snowflake,6,M::Expand), control(S::Icicle,3.5,M::Hold)}),
design("Psychic Bastion", Violet, Azure, 175, {
    cast(S::Eye,A::Hands,5,M::Reveal), cast(S::Mirror,A::Body,9,M::Spin,1),
    active(S::Mirror,A::Hands,4,G::Reflection,M::Pulse,1), active(S::Eye,A::Hands,2.5,G::Sight,M::Pulse)}),
design("Combat Tonic", Jade, Crimson, 52, {
    cast(S::Vial,A::Hands,5,M::Reveal), cast(S::Bubble,A::Hands,6,M::Rise),
    active(S::Vial,A::Hands,2.5,G::Strength,M::Pulse,1), active(S::Fang,A::Hands,2,G::Strength,M::Pulse,1)}),
design("Caustic Splash", Jade, Ember, 249, {
    cast(S::Vial,A::Hands,4,M::Fall,1), cast(S::Pellets,A::Hands,20,M::Expand),
    hit(S::Bubble,6,M::Rise), hit(S::Blast,5,M::Expand,1)}),
design("Antidote Batch", Jade, Ivory, 52, {
    cast(S::Vial,A::Hands,5,M::Reveal), cast(S::Cross,A::Hands,3,M::Expand,1),
    active(S::Bubble,A::Hands,3,G::Regeneration,M::Rise), active(S::Cross,A::Hands,2,G::Regeneration,M::Rise,1)}),
design("Rocket Brew", Ember, Jade, 756, {
    cast(S::Vial,A::Hands,4,M::Rise,1), cast(S::Blast,A::Feet,8,M::Expand),
    active(S::Spark,A::Feet,6,G::Speed,M::Flow), active(S::Vial,A::Hands,2.5,G::Resistance,M::Hold,1)}),
design("Overcharge", Azure, Gold, 171, {
    cast(S::Circuit,A::Hands,7,M::Strike,1), cast(S::Lightning,A::Hands,10,M::Strike),
    active(S::Lightning,A::Hands,5,G::Amplification,M::Pulse), active(S::Steps,A::Feet,5,G::Speed,M::Flow,1)}),
design("Scatter Volley", Ember, Ivory, 239, {
    cast(S::Blast,A::Hands,6,M::Expand), cast(S::Pellets,A::Hands,24,M::Expand,1),
    link(S::Pellets,M::Strike,1), hit(S::Spark,6,M::Shatter)}),
design("Insulation Field", Azure, Gold, 59, {
    cast(S::Circuit,A::Hands,7,M::Reveal,1), cast(S::Hexagon,A::Body,9,M::Expand),
    active(S::Hexagon,A::Hands,4,G::Resistance,M::Hold), active(S::Circuit,A::Hands,3,G::Resistance,M::Pulse,1)}),
design("Arc Discharge", Azure, Ivory, 173, {
    cast(S::Lightning,A::Hands,10,M::Strike,1), cast(S::Wave,A::Feet,-1,M::Expand),
    link(S::Lightning,M::Strike), hit(S::Spark,6,M::Shatter,1), control(S::Lightning,3.5,M::Pulse)}),
design("Sentence", Crimson, Ivory, 26, {
    cast(S::Blade,A::Hands,16,M::Strike), cast(S::Hammer,A::Hands,8,M::Fall,1,0,4), hit(S::Crosscut,7,M::Strike), hit(S::Fang,4,M::Expand)}),
design("Relentless Chase", Crimson, Ivory, 756, {
    cast(S::Banner,A::Hands,5,M::Strike), cast(S::Steps,A::Feet,10,M::Flow,1),
    active(S::Steps,A::Feet,7,G::Speed,M::Flow), active(S::Fang,A::Hands,2,G::Speed,M::Pulse)}),
design("Chain Snare", Ivory, Crimson, 688, {
    cast(S::Chain,A::Hands,12,M::Strike), link(S::Chain,M::Strike,0,.3),
    hit(S::Chain,5,M::Contract), control(S::Chain,5,M::Hold,1)}),
design("Dread Verdict", Crimson, Smoke, 687, {
    cast(S::Hammer,A::Hands,6,M::Strike), cast(S::Sound,A::Hands,12,M::Expand,1),
    active(S::Fang,A::Hands,3,G::Strength,M::Pulse), hit(S::Skull,5,M::Expand,1), control(S::Fang,4,M::Pulse)}),
design("Spirit Surge", Jade, Azure, 752, {
    cast(S::Feather,A::Hands,6,M::Rise), cast(S::Helix,A::Body,7,M::Spin,1),
    active(S::Wind,A::Feet,6,G::Speed,M::Flow), active(S::Well,A::Hands,3,G::Mana,M::Contract,1)}),
design("Entangling Roots", Jade, Gold, 753, {
    cast(S::Root,A::Hands,8,M::Rise), cast(S::Leaf,A::Hands,6,M::Expand,1),
    hit(S::Root,7,M::Rise), control(S::Root,5,M::Hold), control(S::Leaf,3,M::Rise,1)}),
design("Ancestral Remedy", Jade, Ivory, 168, {
    cast(S::Feather,A::Hands,5,M::Rise,1), cast(S::Heart,A::Hands,4,M::Pulse), cast(S::Leaf,A::Body,7,M::Rise,0,0,5)}),
design("Primal Roar", Gold, Jade, 687, {
    cast(S::Fang,A::Hands,6,M::Strike), cast(S::Wave,A::Feet,-1,M::Expand),
    active(S::Feather,A::Hands,3,G::Resistance,M::Hold,1), hit(S::Fang,5,M::Expand), control(S::Skull,3,M::Pulse,1)}),
design("Piercing Shot", Ivory, Gold, 75, {
    cast(S::Arrow,A::Hands,15,M::Strike), cast(S::Compass,A::Hands,4,M::Contract,1),
    link(S::Arrow,M::Strike,0,.2), hit(S::Spear,6,M::Strike,1)}),
design("Hunter's Mark", Jade, Gold, 753, {
    cast(S::Eye,A::Hands,5,M::Reveal), cast(S::Compass,A::Feet,10,M::Spin,1),
    active(S::Compass,A::Hands,3,G::Sight,M::Hold,1), active(S::Eye,A::Hands,2,G::Sight,M::Pulse)}),
design("Crippling Shot", Ivory, Jade, 75, {
    cast(S::Arrow,A::Hands,13,M::Strike), cast(S::Chain,A::Hands,6,M::Contract,1),
    link(S::Arrow,M::Strike), hit(S::Crosscut,4,M::Strike,1), control(S::Steps,4,M::Contract,1)}),
design("Trail Escape", Jade, Smoke, 166, {
    cast(S::Leaf,A::Feet,8,M::Shatter), cast(S::Smoke,A::Body,7,M::Expand,1),
    active(S::Leaf,A::Feet,4,G::Speed,M::Flow), active(S::Leaf,A::Hands,2,G::Hidden,M::Contract,1)}),
design("Battle Ballad", Gold, Crimson, 659, {
    cast(S::Note,A::Hands,5,M::Rise), cast(S::Sound,A::Hands,9,M::Expand,1),
    active(S::Note,A::Hands,3,G::Strength,M::Pulse), active(S::Steps,A::Feet,5,G::Speed,M::Flow,1)}),
design("Lullaby", Azure, Violet, 689, {
    cast(S::Note,A::Hands,5,M::Rise), cast(S::Sound,A::Hands,12,M::Expand,1),
    hit(S::Sleep,4,M::Rise), control(S::Sleep,3,M::Rise), control(S::Note,2.5,M::Rise,1)}),
design("Restorative Verse", Jade, Gold, 690, {
    cast(S::Note,A::Hands,5,M::Rise,1), cast(S::Heart,A::Hands,4,M::Pulse),
    active(S::Note,A::Hands,3,G::Regeneration,M::Rise,1), active(S::Cross,A::Hands,2,G::Regeneration,M::Rise)}),
design("Dissonant Chord", Violet, Crimson, 691, {
    cast(S::Note,A::Hands,5,M::Shatter), cast(S::Sound,A::Hands,12,M::Expand,1),
    hit(S::Note,5,M::Shatter,1), control(S::Confusion,4,M::Spin)}),
design("Breach Charge", Ember, Ivory, 153, {
    cast(S::Circuit,A::Hands,5,M::Strike), cast(S::Blast,A::Hands,15,M::Expand,1,0,4),
    hit(S::Blast,8,M::Expand), hit(S::Spark,6,M::Shatter,1)}),
design("Concussion Burst", Ember, Ivory, 153, {
    cast(S::Blast,A::Hands,7,M::Expand), cast(S::Wave,A::Feet,-1,M::Expand,1),
    hit(S::Sound,5,M::Expand), control(S::Confusion,4,M::Spin)}),
design("Blast Shelter", Ember, Ivory, 44, {
    cast(S::Shield,A::Hands,7,M::Reveal,1), cast(S::Spark,A::Body,7,M::Shatter),
    active(S::Shield,A::Hands,4,G::Resistance,M::Hold,1), active(S::Hexagon,A::Body,5,G::Resistance,M::Pulse)}),
design("Fuse Sprint", Ember, Ivory, 756, {
    cast(S::Spark,A::Hands,5,M::Strike), cast(S::Steps,A::Feet,9,M::Flow,1),
    active(S::Spark,A::Feet,5,G::Speed,M::Flow), active(S::Wind,A::Feet,6,G::Speed,M::Flow,1)}),
design("Ancestral Blade", Violet, Gold, 24, {
    cast(S::Blade,A::Hands,19,M::Sweep), cast(S::Crown,A::Hands,5,M::Reveal,1),
    hit(S::Blade,7,M::Sweep), hit(S::Rune,5,M::Expand,1)}),
design("Bloodline Ward", Violet, Gold, 175, {
    cast(S::Crown,A::Hands,6,M::Reveal,1), cast(S::Mirror,A::Hands,7,M::Strike),
    active(S::Mirror,A::Hands,4,G::Reflection,M::Pulse), active(S::Crown,A::Hands,2.5,G::Resistance,M::Hold,1)}),
design("Noble Resolve", Jade, Azure, 168, {
    cast(S::Crown,A::Hands,5,M::Rise,2), cast(S::Heart,A::Hands,4,M::Pulse), cast(S::Well,A::Body,7,M::Contract,1,0,5)}),
design("Royal Pursuit", Gold, Violet, 756, {
    cast(S::Crown,A::Hands,5,M::Strike), cast(S::Banner,A::Hands,6,M::Flow,1),
    active(S::Steps,A::Feet,6,G::Speed,M::Flow), active(S::Crown,A::Hands,2.5,G::Strength,M::Pulse,1)}),
design("Quiet Restoration", Jade, Azure, 168, {
    cast(S::Leaf,A::Hands,5,M::Rise), cast(S::Well,A::Hands,6,M::Contract,1), cast(S::Heart,A::Body,4,M::Pulse,0,0,6)}),
design("Stone Meditation", Smoke, Ivory, 250, {
    cast(S::Tomb,A::Hands,6,M::Rise), cast(S::Hexagon,A::Feet,10,M::Contract,1),
    active(S::Hexagon,A::Hands,4,G::Resistance,M::Hold,1), active(S::Tomb,A::Hands,2.5,G::Resistance,M::Hold)}),
design("Cave Echo", Smoke, Azure, 66, {
    cast(S::Sound,A::Hands,9,M::Expand,1), cast(S::Wave,A::Feet,-1,M::Expand),
    hit(S::Sound,5,M::Expand,1), control(S::Wave,4,M::Contract)}),
design("Leave No Trace", Smoke, Jade, 166, {
    cast(S::Leaf,A::Feet,7,M::Contract,1), cast(S::Feather,A::Hands,5,M::Rise),
    active(S::Feather,A::Hands,2.5,G::Hidden,M::Rise,1), active(S::Wing,A::Feet,3,G::Levitation,M::Rise)}),
design("Radiant Smite", Gold, Ivory, 165, {
    cast(S::Blade,A::Hands,16,M::Sweep,1), cast(S::Sun,A::Hands,6,M::Strike),
    hit(S::Cross,6,M::Strike,1), hit(S::Sun,7,M::Expand)}),
design("Dawn Aegis", Gold, Jade, 165, {
    cast(S::Sun,A::Hands,6,M::Expand), cast(S::Shield,A::Hands,7,M::Reveal),
    active(S::Shield,A::Hands,4,G::Resistance,M::Hold), active(S::Sun,A::Hands,2.5,G::Regeneration,M::Pulse,1)}),
design("Oath Renewal", Gold, Jade, 168, {
    cast(S::Cross,A::Hands,5,M::Rise), cast(S::Sun,A::Hands,7,M::Expand), cast(S::Heart,A::Body,5,M::Pulse,1,0,5)}),
design("Crusader's Advance", Gold, Ivory, 756, {
    cast(S::Banner,A::Hands,6,M::Strike), cast(S::Wing,A::Feet,7,M::Expand,1),
    active(S::Steps,A::Feet,6,G::Speed,M::Flow,1), active(S::Cross,A::Hands,3,G::Strength,M::Pulse)})
