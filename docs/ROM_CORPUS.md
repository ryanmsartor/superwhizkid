# Known Gamate ROM corpus

This document establishes the cartridge dataset available for static analysis. It reconciles the
current MAME software list with Neo Fuji's historical catalogue, then identifies the exact files in
the local, user-supplied collection by SHA-1.

The inventory was generated on 2026-11-19. Cartridge ROMs and system firmware are catalogued in
separate tables so their different roles and coverage remain clear.

## Scope summary

- Current MAME catalogue: 62 dumped images, including two distinct Bomb Blaster dumps.
- Local collection: 58 cartridge images.
- Exact local matches to the MAME SHA-1 catalogue: 58 of 58.
- Coverage of currently catalogued MAME images: 58 of 62 (93.5%).
- MAME-catalogued dumps absent locally: Mighty Boxer, Kiki Inland, Fortress of Fierceness, and
  Incantational Couple.
- No local cartridge image is unmatched or of uncertain identity.
- Local firmware collection: both recognized 4 KiB BIOS revisions, identified by SHA-1.

`Local SHA-1` is calculated directly from the named local file. An em dash means that the dump is
catalogued by MAME but is not present in the local collection. File sizes are bytes.

## System firmware images

These images are internal system firmware, not cartridges, and are not included in the 58-of-62
cartridge coverage calculation.

| Firmware variant | Size | Local filename | Local SHA-1 |
| --- | ---: | --- | --- |
| UMC/NCR BIOS | 4,096 | `gamate_bios_umc.bin` | `ea449dc607601f9a68d855ad6ab53800d2e99297` |
| Bit Corporation BIOS | 4,096 | `gamate_bios_bit.bin` | `4e9dfbfe916ca485530ef4221593ab68738e2217` |

Both hashes are the recognized firmware revisions already used by the project's two-BIOS boot and
compatibility validation. The files are read locally and are not distributed with Superwhizkid.

## Dumped cartridge images

| Serial | Catalogued title | Size | Local filename | Local SHA-1 |
| --- | --- | ---: | --- | --- |
| C1001 | Cube Up | 16,384 | `Cube Up (Bit Corp) (1990).bin` | `9d3b28887b6f375600bff29f8f233087b9152a60` |
| C1002 | Witty Apee | 32,768 | `Witty Apee (Bit Corp) (1990).bin` | `0c0579472260b1a0f5bfbda4cba8043b2a75aa54` |
| C1003 | Box Forum | 32,768 | `Box Forum (Bit Corp) (1991).bin` | `a4c826d85b990b4c01d17e81b722cc54f7ae9e93` |
| C1004 | Mighty Tank | 32,768 | `Mighty Tank (Bit Corp) (1990).bin` | `ef3a555860b09431fbf1600238e7a4865d7af792` |
| C1005 | Enchanted Bricks | 32,768 | `Enchanted Bricks (Bit Corp) (1990).bin` | `d722c5334a063c1f05e7bcd17fcc645cc3571127` |
| C1006 | Mini Golf | 32,768 | `Mini Golf (Bit Corp) (1990).bin` | `527d6cd2069086e76596bc365be5c807dfffcb64` |
| C1007 | Galaxy Invaders | 32,768 | `Galaxy Invaders (Bit Corp) (1990).bin` | `03f9c3d6cb3192f5bf91f2ac0e578bc65c8b1825` |
| C1008 | Legend of Dragon Knight | 65,536 | `Legend of Dragon Knight (Bit Corp) (1991).bin` | `d01988984e2950fde8096a0156808995a0e5c515` |
| C1009 | Tornado | 32,768 | `Tornado (Bit Corp) (1991).bin` | `671ea650ef462e7dcb4cb02b07ddbc300002707b` |
| C1011 | Money Maze | 32,768 | `Money Maze (Bit Corp) (1990).bin` | `814f4ff6fffb6a86c9936f74f50049b34bc0d362` |
| C1012 | Pharaoh Revenger | 32,768 | `Pharaoh Revenger (Bit Corp) (1991).bin` | `a1b2846613341fe7d92e4ceb40e5525fce5c1cdd` |
| C1013 | Dino Bibo | 32,768 | `Dino Bibo (Bit Corp) (1991).bin` | `7e7ba0b7b919e89e2d8a904de30f5ee34ffba822` |
| C1014 | Time Warrior | 32,768 | `Time Warrior (Bit Corp) (1990).bin` | `8d15793fbea07cfda8d68d7eb96fe0eccc978317` |
| C1015 | Kill Shot | 32,768 | `Kill Shot (Bit Corp) (1991).bin` | `a4310f285f68c1837b4e92a2e127e22e7f0fd4f2` |
| C1016 | Volcano Panic | 131,072 | `Volcano Panic (Bit Corp) (1991).bin` | `958f4936f3aa8db5e76d315ee6acc6aee0e010b4` |
| C1017 | Devil Castle | 32,768 | `Devil Castle (Bit Corp) (1991).bin` | `175994971c157e51ff0b5dd0e46c9e8c92cf2a50` |
| C1018 | Kung-Fu Fighter | 32,768 | `Kung-Fu Fighter (Bit Corp) (1990).bin` | `7188b9265e43d4c51eab016462ced23a2fcb9972` |
| C1019 | Dino Ball | 32,768 | `Dino Ball (Bit Corp) (1991).bin` | `b9c1817bca6e5a580dabd87a4fe8b757c973403f` |
| C1021 | Myth of Asamia | 32,768 | `Myth of Asamia (Bit Corp) (1990).bin` | `70082452deb9d11c91ff3e1ad88ee8cbf24723ad` |
| C1022 | Pipemania | 32,768 | `Pipemania (Bit Corp) (1991).bin` | `a86fb7f4cc0f12b0c2d262a6618df76267c1513e` |
| C1023 | Tennis | 32,768 | `Tennis (Bit Corp) (1991).bin` | `ef5ae26d0f9a744c73eec364ac7bee2fd6eb1ed9` |
| C1024 | Marauder | 32,768 | `Marauder (Bit Corp) (1991).bin` | `f41dab120bf7b3d7e2b21a408c8b89b4224bf825` |
| C1026 | Flipuzzle | 32,768 | `Flipuzzle (Bit Corp) (1991).bin` | `3928cd356f23f595d8023e4d4a46a98308cf4662` |
| C1027 | Monster Pitfall | 32,768 | `Monster Pitfall (Bit Corp) (1990).bin` | `bc35475cfea02de98d2f4646f6ae4a9f48e138f5` |
| C1028 | Vindicators | 32,768 | `Vindicators (Bit Corp) (1990).bin` | `b858479fc168572e684d3bb2330ae6826470f69f` |
| C1029 | Brick Blaster | 32,768 | `Brick Blaster (Bit Corp) (1990).bin` | `a4e2d675ef6fd844b33d0aa40ce75ecaa6e65002` |
| C1031 | Bomb Blaster | 32,768 | `Bomb Blaster (Bit Corp) (1990).bin` | `069928d2577c05a17f42a96f4183a90773e58539` |
| C1031 | Bomb Blaster (alternate dump) | 32,768 | `Bomb Blaster (Alt) (Bit Corp) (1990).bin` | `e79fa833714111a70b4700774dc2575d8cd84f3e` |
| C1032 | Cosmic Fighter | 32,768 | `Cosmic Fighter (Bit Corp) (1990).bin` | `df4ebdd1bfb63fa74b7fe83d0f3ce4b403bb5cde` |
| C1033 | Fist of Thunder | 65,536 | `Fist of Thunder (Bit Corp) (1991).bin` | `899d1592f0da06f9684cb271ec34541c702f21ab` |
| C1035 | Treasure Hunter | 32,768 | `Treasure Hunter (Bit Corp) (1990).bin` | `c724b40b97a7f97abfb33e31e5c315ae986b669a` |
| C1036 | Jewelriss | 32,768 | `Jewelriss (Bit Corp) (1990).bin` | `23f4adad99f6c17fb556b512b6eca2645033aadd` |
| C1037 | Nightmare of Santa Claus | 32,768 | `Nightmare of Santa Claus (Bit Corp) (1991).bin` | `ff3ad53998dd5858f1176767786ec187ea8b059b` |
| C1038 | Mars Voyage | 65,536 | `Mars Voyage (Bit Corp) (1991).bin` | `01163f89447c68554cd2fe60b22dea7b578f1605` |
| C1041 | Mighty Boxer | 131,072 | — | — |
| C1042 | Flying Goblin | 65,536 | `Flying Goblin (Bit Corp) (1991).bin` | `b07681e28b65af53fed23def3f6305ab929bb8f1` |
| C1043 | Boom | 32,768 | `Boom (Bit Corp) (1991).bin` | `02f9a07b251bc29d0e5b3d6a4dfde0ab63bd65a3` |
| C1044 | Snowman Legend | 32,768 | `Snowman Legend (Bit Corp) (1991).bin` | `19184793176d7aba7e353819aca57030254666ae` |
| C1045 | World Cup Soccer | 65,536 | `World Cup Soccer (Bit Corp) (199X).bin` | `148bd0e461d8f967b483136460a770820d2e7226` |
| C1046 | Kiki Inland | 65,536 | — | — |
| C1047 | Fortune 'n Luck | 131,072 | `Fortune 'n Luck (Bit Corp) (1992).bin` | `8719418a6b30c2cc9d013d35d1baacde86016da3` |
| C1048 | Baseball | 65,536 | `Baseball (Bit Corp) (1991).bin` | `f10e180c4aac41e9c710ce38ca9a93ca6f331f57` |
| C1049 | Punk Boy | 65,536 | `Punk Boy (Bit Corp) (1992).bin` | `1b3082be0698c12c8fc978db6a0c63c8958c33f8` |
| C1050 | Fortress of Fierceness | 32,768 | — | — |
| C1051 | Incantational Couple | 32,768 | — | — |
| C1052 | Famous 7 | 65,536 | `Famous 7 (UMC) (1993).bin` | `ea422fe969b260296b99397cdeff6ae053ac4a86` |
| C1053 | Tough Guy | 131,072 | `Tough Guy (UMC) (1993).bin` | `5515f7dc5a00fa38677c4ac5b73af232008f3e51` |
| C1054 | Magic Jigsaw | 32,768 | `Magic Jigsaw (Bit Corp) (1991).bin` | `75004583a83225e8379fa3f34433e0a76222e1f7` |
| C1056 | GP Race | 131,072 | `GP Race (Bit Corp) (1992).bin` | `1b471f8ac8ffb107c62ec3b765c43e268d90b73a` |
| C1057 | Fantasy Travel | 131,072 | `Fantasy Travel (UMC) (1993).bin` | `2eccf1fe8d400a54ffb2f026af8b9842442b6d9f` |
| C1058 | QuizFighter | 262,144 | `QuizFighter (UMC) (1993).bin` | `7d638bf61286adfa612de974efff2d9b5b4b3331` |
| C1060 | Further Adventures of Hannibal The Cat | 524,288 | `Further Adventures of Hannibal The Cat (Bit Corp) (1993).bin` | `a1163f1da52e23bda0502f229488a5b4d04bbe8a` |
| C1061 | Dinosaur Park | 524,288 | `Dinosaur Park (Phinnex) (1993).bin` | `e93c671ee1eb40bb4d7298671dc213a07afef9e0` |
| C1063 | Basketball | 262,144 | `Basketball (Bit Corp) (199X).bin` | `91b9b9060b76d2012d5cbc6523839f67caa37aaf` |
| C1064 | Bao Qing Tian | 524,288 | `Bao Qing Tian (Bit Corp) (1994).bin` | `ba42622b9ff4b2fa0cb70f322ee24b1d8c387222` |
| C1065 | Hot Hero | 524,288 | `Hot Hero (Bit Corp) (199X).bin` | `568f87864c0ed6117fe34e3f79e56c01e911e482` |
| C1067 | Robin Hood | 524,288 | `Robin Hood (Phinnex) (1994).bin` | `bfb2f4b5b771277333ffc1e27cf3a5608e9ea163` |
| C1068 | The Golden Pyramid | 524,288 | `The Golden Pyramid (Bit Corp) (199X).bin` | `69909b78598184760b3e261da897a9bc90af9dfb` |
| C1069 | Riddle of the Ancient Tomb | 524,288 | `Riddle of the Ancient Tomb (Bit Corp) (1994).bin` | `32187903c10dc80d62e9bf794b912921542d6d38` |
| C1070 | Insect War | 524,288 | `Insect War (UMC) (1995).bin` | `6aa0364010272c82c7cc8c7afd60254de148fa72` |
| C1401 | 4-in-1 (Mini Golf, Cube-Up, Brick Blaster, and Vindicators) | 131,072 | `4-in-1 (Mini Golf, Cube-Up, Brick Blaster, and Vindicators) (Bit Corp) (1991).bin` | `4dd20632eb31023e18241290eca7d5a5acb1fa84` |
| K1001 | One Million Whys | 524,288 | `One Million Whys (UMC) (1993).bin` | `beae4f3365031f4e8ae192a5e6d27a28979ef84b` |

## Reported or unresolved cartridges without a catalogued dump

These entries are not part of the static-analysis dataset. MAME records the serial gaps in comments
as undumped and possibly unreleased; Neo Fuji's historical list records names from packaging,
auctions, other lists, or rumors. Their presence here does not assert that a finished cartridge was
released or that a ROM currently exists.

| Serial | Reported identity | Sources and qualification |
| --- | --- | --- |
| C1010 | Bump N' Run | Neo Fuji listing; MAME comment marks it undumped and possibly unreleased. |
| C1020 | Bad Bud Chou Chu's Adventure | Neo Fuji listing; MAME comment marks it undumped and possibly unreleased. |
| C1025 | Jackpot | Neo Fuji listing; MAME comment marks it undumped and possibly unreleased. |
| C1030 | Beach Volleyball | Neo Fuji listing; MAME comment marks it undumped and possibly unreleased. |
| C1034 | Superboy | Neo Fuji listing; MAME comment marks it undumped and possibly unreleased. |
| C1039 | Column #5 | MAME comment marks this title undumped and possibly unreleased. Neo Fuji's older table instead assigns Fortress of Fierceness to C1039; current MAME assigns that dumped game to C1050. |
| C1040 | Unknown | MAME records an unknown undumped slot. Neo Fuji's older table assigns Incantational Couple to C1040; current MAME assigns that dumped game to C1051. |
| C1055 | Unknown | Empty/unknown serial in Neo Fuji and an unknown undumped slot in MAME. |
| C1059 | Unknown | MAME records an unknown undumped slot. |
| C1062 | Unknown | MAME records an unknown undumped slot. |
| C1066 | Unknown | MAME records an unknown undumped slot. |
| Unknown | 2nd Space | Neo Fuji reports it from other game lists and explicitly treats the identification as uncertain. |
| Unknown | Captain Knick Knack | Neo Fuji reports it from other game lists and explicitly treats the identification as uncertain. |
| Unknown | Puppet Knight | Neo Fuji reports it from other game lists and explicitly treats the identification as uncertain. |
| Unknown | Ball Storm | Neo Fuji reports it from other game lists and explicitly treats the identification as uncertain. |
| Unknown | Super Marlo Bros. | Neo Fuji reports it from other game lists and explicitly treats the identification as uncertain. |
| Unknown | Balloon Voyage | Neo Fuji reports it from other game lists and explicitly treats the identification as uncertain. |
| Unknown | Alice in Wonderland | Neo Fuji reports it from other game lists and explicitly treats the identification as uncertain. |

## Catalogue naming differences

- Neo Fuji uses `Cube-Up`; MAME uses `Cube Up` except within the 4-in-1 description.
- Neo Fuji's older catalogue calls C1046 `Kiki Island`; MAME calls it `Kiki Inland`.
- Neo Fuji calls C1053 `Metamorphosiser`; MAME identifies the dumped image as `Tough Guy`.
- Neo Fuji predates or leaves unidentified several later Chinese-titled games that MAME now names
  and assigns to serials.
- These differences are catalogue metadata issues. Local membership is determined by SHA-1, not by
  filename or title spelling.

## Sources and method

- MAME, [`hash/gamate.xml` at commit `954def46685cd0276671138fbd032036b1a771fb`](https://github.com/mamedev/mame/blob/954def46685cd0276671138fbd032036b1a771fb/hash/gamate.xml), accessed 2026-11-19. Used as the dumped-image baseline for serials, canonical catalogue titles, sizes, and reference SHA-1 values; its comments also identify undumped serial gaps. The file is CC0-1.0.
- Neo Fuji, [Gamate Games](https://fuji.12bit.club/gamate/?p=games), accessed 2026-11-19. Used as an independent historical catalogue of confirmed, reported, and rumored cartridges and to preserve older title/serial assignments. Its page explicitly describes its list as compiled from websites, auctions, owned material, and rumors, so it is not treated as a dump-verification database.
- Local user-supplied collection, read in place on 2026-11-19. SHA-1 values were computed with
  `sha1sum`; filenames and hashes are recorded, but no ROM or BIOS bytes were copied into the
  repository.

The MAME list was matched to the local collection by full-file SHA-1. Title-based matching was not
used to establish identity. All 58 local cartridge files matched exactly, including both Bomb
Blaster variants and the 4-in-1 image.
