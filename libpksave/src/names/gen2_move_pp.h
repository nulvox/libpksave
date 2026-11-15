/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 nulvox
 *
 * Gen 2 move base PP table indexed by move ID (0..251).
 * Sourced from pret/pokecrystal data/moves/moves.asm.
 * Index 0 is NO_MOVE and has PP 0.
 */
#ifdef PKSAVE_GEN2
static const uint8_t gen2_move_pp[] = {
    /*   0 */   0, /* ??? */
    /*   1 */  35, /* Pound */
    /*   2 */  25, /* Karate Chop */
    /*   3 */  10, /* DoubleSlap */
    /*   4 */  15, /* Comet Punch */
    /*   5 */  20, /* Mega Punch */
    /*   6 */  20, /* Pay Day */
    /*   7 */  15, /* Fire Punch */
    /*   8 */  15, /* Ice Punch */
    /*   9 */  15, /* ThunderPunch */
    /*  10 */  35, /* Scratch */
    /*  11 */  30, /* ViceGrip */
    /*  12 */   5, /* Guillotine */
    /*  13 */  10, /* Razor Wind */
    /*  14 */  30, /* Swords Dance */
    /*  15 */  30, /* Cut */
    /*  16 */  35, /* Gust */
    /*  17 */  35, /* Wing Attack */
    /*  18 */  20, /* Whirlwind */
    /*  19 */  15, /* Fly */
    /*  20 */  20, /* Bind */
    /*  21 */  20, /* Slam */
    /*  22 */  10, /* Vine Whip */
    /*  23 */  20, /* Stomp */
    /*  24 */  30, /* Double Kick */
    /*  25 */   5, /* Mega Kick */
    /*  26 */  25, /* Jump Kick */
    /*  27 */  15, /* Rolling Kick */
    /*  28 */  15, /* Sand-Attack */
    /*  29 */  15, /* Headbutt */
    /*  30 */  25, /* Horn Attack */
    /*  31 */  20, /* Fury Attack */
    /*  32 */   5, /* Horn Drill */
    /*  33 */  35, /* Tackle */
    /*  34 */  15, /* Body Slam */
    /*  35 */  20, /* Wrap */
    /*  36 */  20, /* Take Down */
    /*  37 */  20, /* Thrash */
    /*  38 */  15, /* Double-Edge */
    /*  39 */  30, /* Tail Whip */
    /*  40 */  35, /* Poison Sting */
    /*  41 */  20, /* Twineedle */
    /*  42 */  20, /* Pin Missile */
    /*  43 */  30, /* Leer */
    /*  44 */  25, /* Bite */
    /*  45 */  40, /* Growl */
    /*  46 */  20, /* Roar */
    /*  47 */  15, /* Sing */
    /*  48 */  20, /* Supersonic */
    /*  49 */  20, /* SonicBoom */
    /*  50 */  20, /* Disable */
    /*  51 */  30, /* Acid */
    /*  52 */  25, /* Ember */
    /*  53 */  15, /* Flamethrower */
    /*  54 */  30, /* Mist */
    /*  55 */  25, /* Water Gun */
    /*  56 */   5, /* Hydro Pump */
    /*  57 */  15, /* Surf */
    /*  58 */  10, /* Ice Beam */
    /*  59 */   5, /* Blizzard */
    /*  60 */  20, /* Psybeam */
    /*  61 */  20, /* BubbleBeam */
    /*  62 */  20, /* Aurora Beam */
    /*  63 */   5, /* Hyper Beam */
    /*  64 */  35, /* Peck */
    /*  65 */  20, /* Drill Peck */
    /*  66 */  25, /* Submission */
    /*  67 */  20, /* Low Kick */
    /*  68 */  20, /* Counter */
    /*  69 */  20, /* Seismic Toss */
    /*  70 */  15, /* Strength */
    /*  71 */  20, /* Absorb */
    /*  72 */  10, /* Mega Drain */
    /*  73 */  10, /* Leech Seed */
    /*  74 */  40, /* Growth */
    /*  75 */  25, /* Razor Leaf */
    /*  76 */  10, /* SolarBeam */
    /*  77 */  35, /* PoisonPowder */
    /*  78 */  30, /* Stun Spore */
    /*  79 */  15, /* Sleep Powder */
    /*  80 */  20, /* Petal Dance */
    /*  81 */  40, /* String Shot */
    /*  82 */  10, /* Dragon Rage */
    /*  83 */  15, /* Fire Spin */
    /*  84 */  30, /* ThunderShock */
    /*  85 */  15, /* Thunderbolt */
    /*  86 */  20, /* Thunder Wave */
    /*  87 */  10, /* Thunder */
    /*  88 */  15, /* Rock Throw */
    /*  89 */  10, /* Earthquake */
    /*  90 */   5, /* Fissure */
    /*  91 */  10, /* Dig */
    /*  92 */  10, /* Toxic */
    /*  93 */  25, /* Confusion */
    /*  94 */  10, /* Psychic */
    /*  95 */  20, /* Hypnosis */
    /*  96 */  40, /* Meditate */
    /*  97 */  30, /* Agility */
    /*  98 */  30, /* Quick Attack */
    /*  99 */  20, /* Rage */
    /* 100 */  20, /* Teleport */
    /* 101 */  15, /* Night Shade */
    /* 102 */  10, /* Mimic */
    /* 103 */  40, /* Screech */
    /* 104 */  15, /* Double Team */
    /* 105 */  20, /* Recover */
    /* 106 */  30, /* Harden */
    /* 107 */  20, /* Minimize */
    /* 108 */  20, /* SmokeScreen */
    /* 109 */  10, /* Confuse Ray */
    /* 110 */  40, /* Withdraw */
    /* 111 */  40, /* Defense Curl */
    /* 112 */  30, /* Barrier */
    /* 113 */  30, /* Light Screen */
    /* 114 */  30, /* Haze */
    /* 115 */  20, /* Reflect */
    /* 116 */  30, /* Focus Energy */
    /* 117 */  10, /* Bide */
    /* 118 */  10, /* Metronome */
    /* 119 */  20, /* Mirror Move */
    /* 120 */   5, /* Selfdestruct */
    /* 121 */  10, /* Egg Bomb */
    /* 122 */  30, /* Lick */
    /* 123 */  20, /* Smog */
    /* 124 */  20, /* Sludge */
    /* 125 */  20, /* Bone Club */
    /* 126 */   5, /* Fire Blast */
    /* 127 */  15, /* Waterfall */
    /* 128 */  10, /* Clamp */
    /* 129 */  20, /* Swift */
    /* 130 */  15, /* Skull Bash */
    /* 131 */  15, /* Spike Cannon */
    /* 132 */  35, /* Constrict */
    /* 133 */  20, /* Amnesia */
    /* 134 */  15, /* Kinesis */
    /* 135 */  10, /* Softboiled */
    /* 136 */  20, /* Hi Jump Kick */
    /* 137 */  30, /* Glare */
    /* 138 */  15, /* Dream Eater */
    /* 139 */  40, /* Poison Gas */
    /* 140 */  20, /* Barrage */
    /* 141 */  15, /* Leech Life */
    /* 142 */  10, /* Lovely Kiss */
    /* 143 */   5, /* Sky Attack */
    /* 144 */  10, /* Transform */
    /* 145 */  30, /* Bubble */
    /* 146 */  10, /* Dizzy Punch */
    /* 147 */  15, /* Spore */
    /* 148 */  20, /* Flash */
    /* 149 */  15, /* Psywave */
    /* 150 */  40, /* Splash */
    /* 151 */  40, /* Acid Armor */
    /* 152 */  10, /* Crabhammer */
    /* 153 */   5, /* Explosion */
    /* 154 */  15, /* Fury Swipes */
    /* 155 */  10, /* Bonemerang */
    /* 156 */  10, /* Rest */
    /* 157 */  10, /* Rock Slide */
    /* 158 */  15, /* Hyper Fang */
    /* 159 */  30, /* Sharpen */
    /* 160 */  30, /* Conversion */
    /* 161 */  10, /* Tri Attack */
    /* 162 */  10, /* Super Fang */
    /* 163 */  20, /* Slash */
    /* 164 */  10, /* Substitute */
    /* 165 */   1, /* Struggle */
    /* 166 */   1, /* Sketch */
    /* 167 */  10, /* Triple Kick */
    /* 168 */  10, /* Thief */
    /* 169 */  10, /* Spider Web */
    /* 170 */   5, /* Mind Reader */
    /* 171 */  15, /* Nightmare */
    /* 172 */  25, /* Flame Wheel */
    /* 173 */  15, /* Snore */
    /* 174 */  10, /* Curse */
    /* 175 */  15, /* Flail */
    /* 176 */  30, /* Conversion2 */
    /* 177 */   5, /* Aeroblast */
    /* 178 */  40, /* Cotton Spore */
    /* 179 */  15, /* Reversal */
    /* 180 */  10, /* Spite */
    /* 181 */  25, /* Powder Snow */
    /* 182 */  10, /* Protect */
    /* 183 */  30, /* Mach Punch */
    /* 184 */  10, /* Scary Face */
    /* 185 */  20, /* Faint Attack */
    /* 186 */  10, /* Sweet Kiss */
    /* 187 */  10, /* Belly Drum */
    /* 188 */  10, /* Sludge Bomb */
    /* 189 */  10, /* Mud-Slap */
    /* 190 */  10, /* Octazooka */
    /* 191 */  20, /* Spikes */
    /* 192 */   5, /* Zap Cannon */
    /* 193 */  40, /* Foresight */
    /* 194 */   5, /* Destiny Bond */
    /* 195 */   5, /* Perish Song */
    /* 196 */  15, /* Icy Wind */
    /* 197 */   5, /* Detect */
    /* 198 */  10, /* Bone Rush */
    /* 199 */   5, /* Lock-On */
    /* 200 */  15, /* Outrage */
    /* 201 */  10, /* Sandstorm */
    /* 202 */   5, /* Giga Drain */
    /* 203 */  10, /* Endure */
    /* 204 */  20, /* Charm */
    /* 205 */  20, /* Rollout */
    /* 206 */  40, /* False Swipe */
    /* 207 */  15, /* Swagger */
    /* 208 */  10, /* Milk Drink */
    /* 209 */  20, /* Spark */
    /* 210 */  20, /* Fury Cutter */
    /* 211 */  25, /* Steel Wing */
    /* 212 */   5, /* Mean Look */
    /* 213 */  15, /* Attract */
    /* 214 */  10, /* Sleep Talk */
    /* 215 */   5, /* Heal Bell */
    /* 216 */  20, /* Return */
    /* 217 */  15, /* Present */
    /* 218 */  20, /* Frustration */
    /* 219 */  25, /* Safeguard */
    /* 220 */  20, /* Pain Split */
    /* 221 */   5, /* Sacred Fire */
    /* 222 */  30, /* Magnitude */
    /* 223 */   5, /* DynamicPunch */
    /* 224 */  10, /* Megahorn */
    /* 225 */  20, /* DragonBreath */
    /* 226 */  40, /* Baton Pass */
    /* 227 */   5, /* Encore */
    /* 228 */  20, /* Pursuit */
    /* 229 */  40, /* Rapid Spin */
    /* 230 */  20, /* Sweet Scent */
    /* 231 */  15, /* Iron Tail */
    /* 232 */  35, /* Metal Claw */
    /* 233 */  10, /* Vital Throw */
    /* 234 */   5, /* Morning Sun */
    /* 235 */   5, /* Synthesis */
    /* 236 */   5, /* Moonlight */
    /* 237 */  15, /* Hidden Power */
    /* 238 */   5, /* Cross Chop */
    /* 239 */  20, /* Twister */
    /* 240 */   5, /* Rain Dance */
    /* 241 */   5, /* Sunny Day */
    /* 242 */  15, /* Crunch */
    /* 243 */  20, /* Mirror Coat */
    /* 244 */  10, /* Psych Up */
    /* 245 */   5, /* ExtremeSpeed */
    /* 246 */   5, /* AncientPower */
    /* 247 */  15, /* Shadow Ball */
    /* 248 */  15, /* Future Sight */
    /* 249 */  15, /* Rock Smash */
    /* 250 */  15, /* Whirlpool */
    /* 251 */  10, /* Beat Up */
};
#endif
