//! Shared text helpers for UI
class AROR_TextUtils
{
	//------------------------------------------------------------------------------------------------
	//! Enum value name to display text: "MILITARY_BASE" -> "Military Base"
	static string FormatEnumName(string enumName)
	{
		array<string> words = {};
		enumName.Split("_", words, true);

		string result;
		foreach (int i, string word : words)
		{
			if (i > 0)
				result += " ";

			// ToLower/ToUpper modify the string in place
			word.ToLower();
			string first = word.Substring(0, 1);
			first.ToUpper();
			result += first + word.Substring(1, word.Length() - 1);
		}

		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! Visible characters in UTF-8 text: Length() counts bytes, so Cyrillic would count double.
	//! UTF-8 continuation bytes (0x80-0xBF) are skipped, whether the engine reports bytes signed or not.
	static int CharCount(string text)
	{
		int count;
		int length = text.Length();
		for (int i = 0; i < length; i++)
		{
			int code = text.Get(i).ToAscii();
			if ((code >= 0x80 && code < 0xC0) || (code >= -128 && code < -64))
				continue;
			count++;
		}
		return count;
	}
}
