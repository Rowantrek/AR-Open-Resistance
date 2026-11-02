//! Colour tokens from the Open Resistance UI design system, packed 0xAARRGGBB.
//! UI code reads colours only from here, e.g. AROR_UIColors.Get(AROR_UIColors.TEXT_MUTED).
class AROR_UIColors
{
	// Ground
	static const int SURFACE = 0xFF0E1210;			//!< HICOMM screen background and full-screen menus
	static const int SURFACE_RAISED = 0xFF16201A;	//!< Cards, list rows and panels on SURFACE
	static const int PLATE = 0xD1080B09;			//!< Every HUD element and toast sits on this (82%)
	static const int PLATE_STRONG = 0xF0080B09;		//!< Alert banners, anything over bright sky (94%)
	static const int LINE = 0xFF2B3A30;				//!< 1px dividers and card borders
	static const int LINE_STRONG = 0xFF5A7765;		//!< Control borders and unselected tabs

	// Text
	static const int TEXT = 0xFFEEF2EE;
	static const int TEXT_MUTED = 0xFFA9B5AD;		//!< Labels, secondary lines, timestamps
	static const int TEXT_FAINT = 0xFF7F8C84;		//!< Disabled text and hints only

	// Meaning
	static const int REBEL = 0xFF7FE39A;			//!< Us: selected states, primary actions, positive results
	static const int REBEL_DIM = 0xFF2F6B42;		//!< Fills behind rebel content, meter tracks. Not for text.
	static const int REBEL_INK = 0xFF06110A;		//!< Text on a REBEL fill
	static const int WARN = 0xFFF2B84B;				//!< Warnings, always with the triangle glyph
	static const int WARN_INK = 0xFF1A1204;
	static const int DANGER = 0xFFFF6A55;			//!< Critical alerts only, always with the diamond glyph
	static const int DANGER_INK = 0xFF1C0805;

	// Factions
	static const int FACTION_USSR = 0xFFE8574C;
	static const int FACTION_US = 0xFF5B9CF0;
	static const int FACTION_CIV = 0xFFC9CDC8;

	static const int DEVICE_BEZEL = 0xFF050706;		//!< The phone body around the screen only

	// MARS has an explicitly requested red identity, separate from alert severity.
	static const int MARS_SURFACE = 0xFF23090E;
	static const int MARS_PANEL = 0xFF381018;
	static const int MARS_LINE = 0xFF712B3A;
	static const int MARS_ACCENT = 0xFFFF8B98;
	static const int MARS_INK = 0xFF23070D;
	static const int MARS_MOTIF = 0xFF511A27;

	// Threat stages (Readiness and Crackdown levels 1-5): how much trouble the Resistance is in
	static const int LEVEL_1 = 0xFF7FE39A;			//!< Quiet (rebel green)
	static const int LEVEL_2 = 0xFFC8E26A;			//!< Watchful (yellow-green)
	static const int LEVEL_3 = 0xFFF2B84B;			//!< Rising (warn amber)
	static const int LEVEL_4 = 0xFFFF8A3D;			//!< Serious (orange)
	static const int LEVEL_5 = 0xFFFF4F3F;			//!< In trouble (red)
	static const int ALERT_CRACKDOWN = 0xFFFF8A3D;	//!< Crackdown-up alert header (orange)

	//------------------------------------------------------------------------------------------------
	//! Tokens are sRGB (as designed); widget colours are linear, so channels are converted (alpha is not)
	static Color Get(int argb)
	{
		return AROR_HiCommMap.Srgb(Red(argb), Green(argb), Blue(argb), Alpha(argb) / 255.0);
	}

	//------------------------------------------------------------------------------------------------
	// Channels 0-255, unpacked with masks and division (no bit shifts; 0xFF... values are negative ints)
	//------------------------------------------------------------------------------------------------
	static int Red(int argb)
	{
		return (argb & 0x00FF0000) / 65536;
	}

	//------------------------------------------------------------------------------------------------
	static int Green(int argb)
	{
		return (argb & 0x0000FF00) / 256;
	}

	//------------------------------------------------------------------------------------------------
	static int Blue(int argb)
	{
		return argb & 0x000000FF;
	}

	//------------------------------------------------------------------------------------------------
	static int Alpha(int argb)
	{
		int alpha = (argb & 0x7F000000) / 16777216;
		if (argb < 0)
			alpha += 128;
		return alpha;
	}

	//------------------------------------------------------------------------------------------------
	//! Readiness/Crackdown stage colour: green at 1 (quiet), through yellow and amber, to red at 5 (in trouble)
	static int ForLevel(int level)
	{
		switch (level)
		{
			case 1: return LEVEL_1;
			case 2: return LEVEL_2;
			case 3: return LEVEL_3;
			case 4: return LEVEL_4;
			case 5: return LEVEL_5;
		}
		return TEXT_MUTED;
	}
}
