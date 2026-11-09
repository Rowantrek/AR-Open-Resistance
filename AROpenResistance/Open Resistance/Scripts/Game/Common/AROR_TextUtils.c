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
	//! English text with Latin letters swapped for Cyrillic look-alikes, so it reads as English
	//! but looks Russian: "Invaders" -> "Иnvаdеяs". Letters without a good look-alike are kept.
	static string Cyrillicize(string text)
	{
		string latin = "ABCEHKMNOPRTXYaceopxy";
		array<string> cyrillic = {"А", "В", "С", "Е", "Н", "К", "М", "И", "О", "Р", "Я", "Т", "Х", "У", "а", "с", "е", "о", "р", "х", "у"};

		string result;
		for (int i = 0; i < text.Length(); i++)
		{
			string letter = text.Substring(i, 1);
			int index = latin.IndexOf(letter);
			if (index >= 0)
				result += cyrillic[index];
			else
				result += letter;
		}

		return result;
	}
}
