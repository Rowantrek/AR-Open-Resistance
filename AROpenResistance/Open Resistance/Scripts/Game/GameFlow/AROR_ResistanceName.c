//! The rule for the Resistance name, chosen at campaign setup and changed in HICOMM. The client checks it live and the
//! server checks it again before accepting a name: letters and digits, single spaces between words, at most MAX_LENGTH.
class AROR_ResistanceName
{
	static const int MAX_LENGTH = 32;

	protected static const string ALLOWED = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 ";

	//------------------------------------------------------------------------------------------------
	//! True when the name may be used; otherwise reason says why
	static bool Validate(string name, out string reason)
	{
		reason = string.Empty;
		if (name.IsEmpty())
		{
			reason = "The resistance needs a name";
			return false;
		}

		if (name.Length() > MAX_LENGTH)
		{
			reason = string.Format("The name can be at most %1 characters", MAX_LENGTH);
			return false;
		}

		for (int i = 0, count = name.Length(); i < count; i++)
		{
			if (ALLOWED.IndexOf(name.Get(i)) < 0)
			{
				reason = "Only letters A-Z, digits 0-9 and spaces are allowed";
				return false;
			}
		}

		if (name.Get(0) == " " || name.Get(name.Length() - 1) == " ")
		{
			reason = "The name cannot start or end with a space";
			return false;
		}

		if (name.IndexOf("  ") >= 0)
		{
			reason = "Use single spaces between words";
			return false;
		}

		return true;
	}
}
