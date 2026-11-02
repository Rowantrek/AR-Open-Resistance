//! Portrait Bank Transfer screen: personal balance, recipient picker (Resistance Fund + online players), amount, confirm.
class AROR_BankTransferApp
{
	protected static const ResourceName TRANSFER_SCREEN = "{D42566800A9BC86E}UI/Textures/Phone/Apps/Bank/AROR_Bank_Transfer.edds";
	protected static const string OPTION_PREFIX = "BankTransferOptionButton";

	protected static const int OPTION_POOL = 16;
	protected static const int RESISTANCE_SENTINEL = -1;

	protected static const float SCREEN_WIDTH = 900;
	protected static const float SCREEN_HEIGHT = 1700;

	protected static const float BACK_X = 60;
	protected static const float BACK_Y = 1480;
	protected static const float BACK_WIDTH = 370;
	protected static const float BACK_HEIGHT = 120;

	protected static const float CONFIRM_X = 470;
	protected static const float CONFIRM_Y = 1480;
	protected static const float CONFIRM_WIDTH = 370;
	protected static const float CONFIRM_HEIGHT = 120;

	protected static const float CONTENT_X = 60;
	protected static const float CONTENT_Y = 370;
	protected static const float CONTENT_WIDTH = 780;

	protected static const float FIELD_HEIGHT = 90;
	protected static const float OPTION_HEIGHT = 80;
	protected static const float LABEL_HEIGHT = 40;
	protected static const float CHAR_WIDTH_FACTOR = 0.55;

	protected static const int COLOR_GOLD = 0xFFFDB813;
	protected static const int COLOR_FIELD = 0xFF152238;
	protected static const int COLOR_OPTION = 0xFF1A2A44;
	protected static const int COLOR_OPTION_SELECTED = 0xFF2A3F66;
	protected static const int COLOR_MUTED = 0xFFA8B4C4;

	protected Widget m_wRoot;
	protected ImageWidget m_wBackground;
	protected RichTextWidget m_wBalanceLabel;
	protected RichTextWidget m_wBalance;
	protected RichTextWidget m_wToLabel;
	protected ImageWidget m_wRecipientBg;
	protected RichTextWidget m_wRecipientLabel;
	protected RichTextWidget m_wRecipientCaret;
	protected Widget m_wRecipientButton;
	protected RichTextWidget m_wAmountLabel;
	protected ImageWidget m_wAmountBg;
	protected EditBoxWidget m_wAmountEdit;
	protected Widget m_wBackButton;
	protected Widget m_wConfirmButton;

	protected ref array<ImageWidget> m_aOptionBgs = {};
	protected ref array<RichTextWidget> m_aOptionLabels = {};
	protected ref array<Widget> m_aOptionButtons = {};

	//! Parallel to visible dropdown rows: player ID, or RESISTANCE_SENTINEL
	protected ref array<int> m_aOptionIds = {};

	protected bool m_bDropdownOpen;
	protected bool m_bToResistance;
	protected int m_iTargetPlayerId;
	protected bool m_bHasSelection;
	protected bool m_bAmountSeeded;
	protected int m_iNextRefresh;

	//------------------------------------------------------------------------------------------------
	bool Init(Widget phoneRoot)
	{
		m_wRoot = phoneRoot.FindAnyWidget("BankTransferApp");
		m_wBackground = ImageWidget.Cast(phoneRoot.FindAnyWidget("BankTransferBackground"));
		m_wBalanceLabel = RichTextWidget.Cast(phoneRoot.FindAnyWidget("BankTransferBalanceLabel"));
		m_wBalance = RichTextWidget.Cast(phoneRoot.FindAnyWidget("BankTransferBalance"));
		m_wToLabel = RichTextWidget.Cast(phoneRoot.FindAnyWidget("BankTransferToLabel"));
		m_wRecipientBg = ImageWidget.Cast(phoneRoot.FindAnyWidget("BankTransferRecipientBg"));
		m_wRecipientLabel = RichTextWidget.Cast(phoneRoot.FindAnyWidget("BankTransferRecipientLabel"));
		m_wRecipientCaret = RichTextWidget.Cast(phoneRoot.FindAnyWidget("BankTransferRecipientCaret"));
		m_wRecipientButton = phoneRoot.FindAnyWidget("BankTransferRecipientButton");
		m_wAmountLabel = RichTextWidget.Cast(phoneRoot.FindAnyWidget("BankTransferAmountLabel"));
		m_wAmountBg = ImageWidget.Cast(phoneRoot.FindAnyWidget("BankTransferAmountBg"));
		m_wAmountEdit = EditBoxWidget.Cast(phoneRoot.FindAnyWidget("BankTransferAmountEdit"));
		m_wBackButton = phoneRoot.FindAnyWidget("BankTransferBackButton");
		m_wConfirmButton = phoneRoot.FindAnyWidget("BankTransferConfirmButton");

		if (!m_wRoot || !m_wBackground || !m_wBalanceLabel || !m_wBalance || !m_wToLabel || !m_wRecipientBg
			|| !m_wRecipientLabel || !m_wRecipientCaret || !m_wRecipientButton || !m_wAmountLabel
			|| !m_wAmountBg || !m_wAmountEdit || !m_wBackButton || !m_wConfirmButton)
		{
			return false;
		}

		for (int i = 0; i < OPTION_POOL; i++)
		{
			ImageWidget bg = ImageWidget.Cast(phoneRoot.FindAnyWidget("BankTransferOptionBg" + i));
			RichTextWidget label = RichTextWidget.Cast(phoneRoot.FindAnyWidget("BankTransferOptionLabel" + i));
			Widget button = phoneRoot.FindAnyWidget("BankTransferOptionButton" + i);
			if (!bg || !label || !button)
				return false;

			m_aOptionBgs.Insert(bg);
			m_aOptionLabels.Insert(label);
			m_aOptionButtons.Insert(button);
		}

		m_wBackground.LoadImageTexture(0, TRANSFER_SCREEN);
		m_wRecipientBg.SetColor(Color.FromInt(COLOR_FIELD));
		m_wAmountBg.SetColor(Color.FromInt(COLOR_FIELD));

		StyleText(m_wBalanceLabel, "CURRENT BALANCE", false, COLOR_GOLD);
		StyleText(m_wBalance, "$ -", true, COLOR_GOLD);
		StyleText(m_wToLabel, "TO", false, COLOR_GOLD);
		StyleText(m_wRecipientLabel, "Select recipient", false, COLOR_MUTED);
		StyleText(m_wRecipientCaret, "v", true, COLOR_GOLD);
		StyleText(m_wAmountLabel, "AMOUNT", false, COLOR_GOLD);

		for (int i = 0; i < OPTION_POOL; i++)
		{
			m_aOptionBgs[i].SetColor(Color.FromInt(COLOR_OPTION));
			StyleText(m_aOptionLabels[i], string.Empty, false, 0xFFF5F5F8);
		}

		array<Widget> invisible = {
			m_wRecipientButton, m_wBackButton, m_wConfirmButton
		};
		foreach (Widget button : invisible)
		{
			button.SetColor(Color.FromRGBA(0, 0, 0, 0));
		}

		foreach (Widget optionButton : m_aOptionButtons)
		{
			optionButton.SetColor(Color.FromRGBA(0, 0, 0, 0));
		}

		ResetForm();
		SetVisible(false, 1);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Invisible hit targets the phone menu should register with its click handler
	void CollectButtons(notnull array<Widget> outButtons)
	{
		outButtons.Insert(m_wRecipientButton);
		outButtons.Insert(m_wBackButton);
		outButtons.Insert(m_wConfirmButton);
		foreach (Widget button : m_aOptionButtons)
		{
			outButtons.Insert(button);
		}
	}

	//------------------------------------------------------------------------------------------------
	void SetVisible(bool visible, float opacity)
	{
		m_wRoot.SetVisible(visible);
		m_wRoot.SetEnabled(visible);
		m_wRoot.SetOpacity(opacity);
		if (!visible)
		{
			m_bDropdownOpen = false;
			m_iNextRefresh = 0;
			HideOptions();
			// Collapse so a hidden full-screen frame cannot steal clicks from Bank home
			Place(m_wRoot, 0, 0, 0, 0);
		}
		else if (!m_bAmountSeeded && m_wAmountEdit)
		{
			m_wAmountEdit.SetText(string.Empty);
			m_bAmountSeeded = true;
		}
	}

	//------------------------------------------------------------------------------------------------
	void OnTransferSucceeded()
	{
		if (m_wAmountEdit)
			m_wAmountEdit.SetText(string.Empty);

		m_bDropdownOpen = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Returns true if the click was handled (including Back, which the menu still routes to Bank)
	bool OnButton(string buttonName)
	{
		if (buttonName == "BankTransferRecipientButton")
		{
			m_bDropdownOpen = !m_bDropdownOpen;
			if (m_bDropdownOpen)
				RefreshRecipients();
			return true;
		}

		if (buttonName == "BankTransferConfirmButton")
		{
			ConfirmTransfer();
			return true;
		}

		if (buttonName.IndexOf(OPTION_PREFIX) == 0)
		{
			int index = buttonName.Substring(OPTION_PREFIX.Length(), buttonName.Length() - OPTION_PREFIX.Length()).ToInt();
			SelectOption(index);
			return true;
		}

		return buttonName == "BankTransferBackButton";
	}

	//------------------------------------------------------------------------------------------------
	void Layout(float left, float top, float scale)
	{
		int now = GetGame().GetWorld().GetWorldTime();
		if (now >= m_iNextRefresh)
		{
			if (m_bDropdownOpen)
				RefreshRecipients();

			m_iNextRefresh = now + 1000;
		}

		Place(m_wRoot, left, top, SCREEN_WIDTH * scale, SCREEN_HEIGHT * scale);
		Place(m_wBackground, 0, 0, SCREEN_WIDTH * scale, SCREEN_HEIGHT * scale);

		string balance = "$ -";
		AROR_EconomyComponent economy = AROR_EconomyComponent.GetInstance();
		if (economy)
			balance = AROR_EconomyComponent.FormatMoney(economy.GetLocalPlayerMoney());

		m_wBalance.SetText(balance);
		UpdateRecipientLabel();
		if (m_wRecipientCaret)
		{
			if (m_bDropdownOpen)
				m_wRecipientCaret.SetText("^");
			else
				m_wRecipientCaret.SetText("v");
		}

		float y = CONTENT_Y;
		Label(m_wBalanceLabel, CONTENT_X, y, CONTENT_WIDTH, LABEL_HEIGHT, 28, scale);
		y += LABEL_HEIGHT + 10;
		Label(m_wBalance, CONTENT_X, y, CONTENT_WIDTH, 70, FitFontSize(balance, 56, CONTENT_WIDTH), scale);
		y += 90;

		Label(m_wToLabel, CONTENT_X, y, CONTENT_WIDTH, LABEL_HEIGHT, 28, scale);
		y += LABEL_HEIGHT + 10;

		float recipientY = y;
		Place(m_wRecipientBg, CONTENT_X * scale, recipientY * scale, CONTENT_WIDTH * scale, FIELD_HEIGHT * scale);
		Label(m_wRecipientLabel, CONTENT_X + 24, recipientY, CONTENT_WIDTH - 90, FIELD_HEIGHT, 34, scale);
		Label(m_wRecipientCaret, CONTENT_X + CONTENT_WIDTH - 70, recipientY, 50, FIELD_HEIGHT, 28, scale);
		Place(m_wRecipientButton, CONTENT_X * scale, recipientY * scale, CONTENT_WIDTH * scale, FIELD_HEIGHT * scale);
		y += FIELD_HEIGHT + 36;

		Label(m_wAmountLabel, CONTENT_X, y, CONTENT_WIDTH, LABEL_HEIGHT, 28, scale);
		y += LABEL_HEIGHT + 10;

		float amountY = y;
		Place(m_wAmountBg, CONTENT_X * scale, amountY * scale, CONTENT_WIDTH * scale, FIELD_HEIGHT * scale);
		Place(m_wAmountEdit, (CONTENT_X + 20) * scale, (amountY + 18) * scale, (CONTENT_WIDTH - 40) * scale, (FIELD_HEIGHT - 30) * scale);
		m_wAmountEdit.SetVisible(!m_bDropdownOpen);

		Place(m_wBackButton, BACK_X * scale, BACK_Y * scale, BACK_WIDTH * scale, BACK_HEIGHT * scale);
		Place(m_wConfirmButton, CONFIRM_X * scale, CONFIRM_Y * scale, CONFIRM_WIDTH * scale, CONFIRM_HEIGHT * scale);

		LayoutOptions(recipientY + FIELD_HEIGHT + 8, scale);
	}

	//------------------------------------------------------------------------------------------------
	protected void LayoutOptions(float startY, float scale)
	{
		int count = m_aOptionIds.Count();
		for (int i = 0; i < OPTION_POOL; i++)
		{
			bool show = m_bDropdownOpen && i < count;
			m_aOptionBgs[i].SetVisible(show);
			m_aOptionLabels[i].SetVisible(show);
			m_aOptionButtons[i].SetVisible(show);
			if (!show)
				continue;

			float y = startY + i * (OPTION_HEIGHT + 4);
			// Keep the list above the footer buttons
			if (y + OPTION_HEIGHT > BACK_Y - 20)
			{
				m_aOptionBgs[i].SetVisible(false);
				m_aOptionLabels[i].SetVisible(false);
				m_aOptionButtons[i].SetVisible(false);
				continue;
			}

			bool selected = IsOptionSelected(m_aOptionIds[i]);
			if (selected)
				m_aOptionBgs[i].SetColor(Color.FromInt(COLOR_OPTION_SELECTED));
			else
				m_aOptionBgs[i].SetColor(Color.FromInt(COLOR_OPTION));

			Place(m_aOptionBgs[i], CONTENT_X * scale, y * scale, CONTENT_WIDTH * scale, OPTION_HEIGHT * scale);
			Label(m_aOptionLabels[i], CONTENT_X + 24, y, CONTENT_WIDTH - 48, OPTION_HEIGHT, 32, scale);
			Place(m_aOptionButtons[i], CONTENT_X * scale, y * scale, CONTENT_WIDTH * scale, OPTION_HEIGHT * scale);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void HideOptions()
	{
		for (int i = 0; i < OPTION_POOL; i++)
		{
			m_aOptionBgs[i].SetVisible(false);
			m_aOptionLabels[i].SetVisible(false);
			m_aOptionButtons[i].SetVisible(false);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshRecipients()
	{
		m_aOptionIds.Clear();
		m_aOptionIds.Insert(RESISTANCE_SENTINEL);

		int localId = GetLocalPlayerId();
		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			if (playerId == localId)
				continue;

			if (m_aOptionIds.Count() >= OPTION_POOL)
				break;

			m_aOptionIds.Insert(playerId);
		}

		for (int i = 0; i < OPTION_POOL; i++)
		{
			if (i >= m_aOptionIds.Count())
			{
				m_aOptionLabels[i].SetText(string.Empty);
				continue;
			}

			m_aOptionLabels[i].SetText(OptionName(m_aOptionIds[i]));
		}

		// Drop a stale player selection if they left
		if (m_bHasSelection && !m_bToResistance && !m_aOptionIds.Contains(m_iTargetPlayerId))
		{
			m_bHasSelection = false;
			m_iTargetPlayerId = 0;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void SelectOption(int index)
	{
		if (index < 0 || index >= m_aOptionIds.Count())
			return;

		int id = m_aOptionIds[index];
		m_bHasSelection = true;
		m_bToResistance = id == RESISTANCE_SENTINEL;
		if (m_bToResistance)
			m_iTargetPlayerId = 0;
		else
			m_iTargetPlayerId = id;

		m_bDropdownOpen = false;
		UpdateRecipientLabel();
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateRecipientLabel()
	{
		if (!m_wRecipientLabel)
			return;

		if (!m_bHasSelection)
		{
			StyleText(m_wRecipientLabel, "Select recipient", false, COLOR_MUTED);
			return;
		}

		int optionId = m_iTargetPlayerId;
		if (m_bToResistance)
			optionId = RESISTANCE_SENTINEL;

		StyleText(m_wRecipientLabel, OptionName(optionId), true, 0xFFF5F5F8);
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsOptionSelected(int optionId)
	{
		if (!m_bHasSelection)
			return false;

		if (m_bToResistance)
			return optionId == RESISTANCE_SENTINEL;

		return optionId == m_iTargetPlayerId;
	}

	//------------------------------------------------------------------------------------------------
	protected static string OptionName(int optionId)
	{
		if (optionId == RESISTANCE_SENTINEL)
			return "Resistance Fund";

		return GetGame().GetPlayerManager().GetPlayerName(optionId);
	}

	//------------------------------------------------------------------------------------------------
	protected void ConfirmTransfer()
	{
		if (!m_bHasSelection)
		{
			AROR_NotificationManagerComponent.ShowHint("Choose a recipient");
			return;
		}

		int amount = ParseAmount(m_wAmountEdit.GetText());
		if (amount <= 0)
		{
			AROR_NotificationManagerComponent.ShowHint("Enter a valid amount");
			return;
		}

		AROR_EconomyComponent economy = AROR_EconomyComponent.GetInstance();
		if (economy && economy.GetLocalPlayerMoney() < amount)
		{
			AROR_NotificationManagerComponent.ShowHint("Insufficient funds");
			return;
		}

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			AROR_NotificationManagerComponent.ShowHint("Transfer failed");
			return;
		}

		controller.AROR_RequestBankTransfer(m_iTargetPlayerId, m_bToResistance, amount);
	}

	//------------------------------------------------------------------------------------------------
	protected void ResetForm()
	{
		m_bDropdownOpen = false;
		m_bToResistance = false;
		m_iTargetPlayerId = 0;
		m_bHasSelection = false;
		m_bAmountSeeded = false;
		m_aOptionIds.Clear();
		UpdateRecipientLabel();
		HideOptions();
	}

	//------------------------------------------------------------------------------------------------
	//! Digits only; strips currency decoration. Invalid text yields 0.
	protected static int ParseAmount(string text)
	{
		if (text.IsEmpty())
			return 0;

		string digits;
		int length = text.Length();
		for (int i = 0; i < length; i++)
		{
			string ch = text.Substring(i, 1);
			if (ch == "0" || ch == "1" || ch == "2" || ch == "3" || ch == "4"
				|| ch == "5" || ch == "6" || ch == "7" || ch == "8" || ch == "9")
			{
				digits += ch;
			}
		}

		if (digits.IsEmpty())
			return 0;

		return digits.ToInt();
	}

	//------------------------------------------------------------------------------------------------
	protected static int GetLocalPlayerId()
	{
		PlayerController controller = GetGame().GetPlayerController();
		if (!controller)
			return 0;

		return controller.GetPlayerId();
	}

	//------------------------------------------------------------------------------------------------
	protected static float FitFontSize(string text, float fontSize, float maxWidth)
	{
		int length = text.Length();
		if (length == 0)
			return fontSize;

		return Math.Min(fontSize, maxWidth / (length * CHAR_WIDTH_FACTOR));
	}

	//------------------------------------------------------------------------------------------------
	protected static void StyleText(TextWidget text, string content, bool bold, int color)
	{
		if (!text)
			return;

		text.SetText(content);
		text.SetColor(Color.FromInt(color));
		text.SetBold(bold);
		text.SetShadow(1, 0xFF000000, 0.35, 1, 1);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Label(TextWidget text, float x, float y, float width, float height, float fontSize, float scale)
	{
		if (!text)
			return;

		text.SetExactFontSize(Math.Max(1, Math.Round(fontSize * scale)));
		Place(text, x * scale, y * scale, width * scale, height * scale);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Place(Widget widget, float x, float y, float width, float height)
	{
		if (!widget)
			return;

		FrameSlot.SetPos(widget, x, y);
		FrameSlot.SetSize(widget, width, height);
	}
}
