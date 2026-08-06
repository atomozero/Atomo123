#include "AtomTheme.h"

// Colori del brand presi da ui/icons/atomo123.svg: blu #2f6fed e
// arancione #ff8a3d.

const AtomTheme kTheme = {
	{ 12, 17, 30, 255 },   { 3, 5, 11, 255 },      // bg gradient
	{ 255, 138, 61, 255 },                         // nucleus (brand orange)
	{ 47, 111, 237, 255 },                          // orbit (brand blue)
	{ 130, 180, 255, 255 },                         // electron
	{ 60, 130, 255, 255 },                           // glow tint
	{ 246, 248, 255, 255 },                          // title
	{ 168, 184, 214, 255 },                          // subtitle
	{ 17, 23, 38, 255 },                             // band fill
	{ 47, 111, 237, 80 },                            // band top separator line
	true
};
