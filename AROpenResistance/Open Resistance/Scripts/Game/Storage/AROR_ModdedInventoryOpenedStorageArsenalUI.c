//! Passes clicks on the arsenal's category buttons to the arsenal panel
class AROR_ArsenalTabHandler : ScriptedWidgetEventHandler
{
	protected SCR_InventoryOpenedStorageArsenalUI m_Ui;

	//------------------------------------------------------------------------------------------------
	void AROR_ArsenalTabHandler(SCR_InventoryOpenedStorageArsenalUI ui)
	{
		m_Ui = ui;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (m_Ui)
			m_Ui.AROR_OnTabClick(w);

		return true;
	}
}

//! A Resistance arsenal panel has a column of category buttons (layout widget "Tabs") on its left; the selected category
//! expands into its sub-types, and only the selection is listed. The vanilla grid pages the filtered list
//! (see ARCHITECTURE.md -> Resistance Storage).
modded class SCR_InventoryOpenedStorageArsenalUI
{
	//! A size layout "Tab" with an image "Bg", a text "Label" and a button "Button" (in that order, in a frame)
	protected static const ResourceName AROR_TAB_LAYOUT = "{039ABEC95EB562AD}UI/Layouts/Arsenal/AROR_ArsenalTab.layout";

	protected static const float AROR_COLUMN_WIDTH = 140;
	protected static const float AROR_COLUMN_GAP = 6;		//!< Between the column and the panel
	protected static const float AROR_TOP_OFFSET = 64;		//!< Height of the panel's title bar
	protected static const float AROR_CATEGORY_HEIGHT = 32;
	protected static const float AROR_TYPE_HEIGHT = 28;
	protected static const float AROR_ROW_GAP = 2;
	protected static const float AROR_TYPE_INDENT = 16;
	protected static const float AROR_CATEGORY_FONT = 16;
	protected static const float AROR_TYPE_FONT = 14;
	protected static const int AROR_CATEGORY_BUTTON = -2;		//!< Marks a category button (vs a sub-type button)

	protected ref AROR_ArsenalTabHandler m_ARORTabHandler;
	protected ref AROR_ArsenalCatalog m_ARORCatalog;

	protected static const int AROR_CATEGORY_UNSET = AROR_ArsenalCatalog.CATEGORY_ALL - 1;	//!< below every real value = not chosen yet

	//! Last selection, so reopening an arsenal returns to the same tab
	protected static int s_iARORLastCategory = AROR_CATEGORY_UNSET;
	protected static int s_iARORLastType = AROR_ArsenalCatalog.TYPE_ALL;

	protected int m_iARORCategory = AROR_CATEGORY_UNSET;
	protected int m_iARORType = AROR_ArsenalCatalog.TYPE_ALL;

	protected ref array<int> m_aARORCategories = {};	//!< Categories to show, in button order
	protected ref array<int> m_aARORTypes = {};			//!< Sub-types of the selected category (with "All" first), empty when it has one
	protected ref array<Widget> m_aARORButtons = {};
	protected ref array<int> m_aARORButtonCategory = {};
	protected ref array<int> m_aARORButtonType = {};	//!< AROR_CATEGORY_BUTTON for category buttons

	//------------------------------------------------------------------------------------------------
	protected bool AROR_IsResistance()
	{
		return m_Storage && m_Storage.GetOwner() && AROR_ResistanceArsenalComponent.IsResistanceArsenal(m_Storage.GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	protected void AROR_GetStockedPrefabs(notnull array<ResourceName> outPrefabs)
	{
		SCR_ArsenalComponent arsenal = SCR_ArsenalComponent.Cast(m_Storage.GetOwner().FindComponent(SCR_ArsenalComponent));
		if (arsenal)
			arsenal.GetAvailablePrefabs(outPrefabs);
	}

	//------------------------------------------------------------------------------------------------
	override void Init()
	{
		// Set before super.Init(), which lists the items through GetAllItems
		m_iARORCategory = s_iARORLastCategory;
		m_iARORType = s_iARORLastType;

		super.Init();

		if (!AROR_IsResistance())
			return;

		// Vanilla only subscribes to arsenal updates when the layout has its supply widgets, which ours lacks, so listen
		// to the stock directly (and drop any vanilla subscription so a change does not refresh twice)
		SCR_ArsenalComponent arsenal = SCR_ArsenalComponent.Cast(m_Storage.GetOwner().FindComponent(SCR_ArsenalComponent));
		if (arsenal)
			arsenal.GetOnArsenalUpdated().Remove(OnArsenalUpdated);

		AROR_ResistanceStorageComponent stock = AROR_ResistanceStorageComponent.GetInstance();
		if (stock)
			stock.GetOnStorageChanged().Insert(AROR_OnStockChanged);

		array<ResourceName> prefabs = {};
		AROR_GetStockedPrefabs(prefabs);
		AROR_PrepareTabs(prefabs);
		AROR_ShowTabs();
	}

	//------------------------------------------------------------------------------------------------
	override void Refresh()
	{
		super.Refresh();

		if (AROR_IsResistance())
			AROR_ShowTabs();
	}

	//------------------------------------------------------------------------------------------------
	//! Several stock changes in one frame (e.g. a deposit unpacking a weapon and its magazine) refresh once
	protected void AROR_OnStockChanged()
	{
		GetGame().GetCallqueue().Remove(Refresh);
		GetGame().GetCallqueue().CallLater(Refresh, 0);
	}

	//------------------------------------------------------------------------------------------------
	override event void HandlerDeattached(Widget w)
	{
		AROR_ResistanceStorageComponent stock = AROR_ResistanceStorageComponent.GetInstance();
		if (stock)
			stock.GetOnStorageChanged().Remove(AROR_OnStockChanged);

		GetGame().GetCallqueue().Remove(Refresh);

		m_aARORButtons.Clear();
		m_aARORButtonCategory.Clear();
		m_aARORButtonType.Clear();
		super.HandlerDeattached(w);
	}

	//------------------------------------------------------------------------------------------------
	//! Keeps the selection valid for the current stock and works out the buttons to show
	protected void AROR_PrepareTabs(notnull array<ResourceName> prefabs)
	{
		// The category lookup only depends on the enabled content packs, so build it once per panel
		if (!m_ARORCatalog)
			m_ARORCatalog = AROR_ArsenalCatalog.Build();

		set<int> stocked = new set<int>();
		foreach (ResourceName prefab : prefabs)
		{
			stocked.Insert(m_ARORCatalog.GetCategory(prefab));
		}

		// Every category regardless of stock, in tab order; Other only when something falls in it
		m_aARORCategories.Clear();
		AROR_ArsenalCatalog.GetCategories(m_aARORCategories);
		if (stocked.Contains(AROR_ArsenalCatalog.CATEGORY_OTHER))
			m_aARORCategories.Insert(AROR_ArsenalCatalog.CATEGORY_OTHER);

		if (!m_aARORCategories.IsEmpty() && m_aARORCategories.Find(m_iARORCategory) < 0)
		{
			// Open on the first stocked category rather than All: listing all stock builds a slot per item
			m_iARORCategory = m_aARORCategories[0];
			foreach (int category : m_aARORCategories)
			{
				if (category != AROR_ArsenalCatalog.CATEGORY_ALL && stocked.Contains(category))
				{
					m_iARORCategory = category;
					break;
				}
			}

			m_iARORType = AROR_ArsenalCatalog.TYPE_ALL;
		}

		array<int> types = {};
		m_ARORCatalog.GetTypes(prefabs, m_iARORCategory, types);
		if (m_iARORType != AROR_ArsenalCatalog.TYPE_ALL && types.Find(m_iARORType) < 0)
			m_iARORType = AROR_ArsenalCatalog.TYPE_ALL;

		m_aARORTypes.Clear();
		if (types.Count() > 1)	// a single sub-type adds nothing over "All"
		{
			m_aARORTypes.Insert(AROR_ArsenalCatalog.TYPE_ALL);
			m_aARORTypes.InsertAll(types);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Rebuilds the category buttons inside the layout's "Tabs" column
	protected void AROR_ShowTabs()
	{
		Widget list = m_widget.FindAnyWidget("Tabs");
		if (!list)
			return;

		SizeLayoutWidget column = SizeLayoutWidget.Cast(m_widget.FindAnyWidget("TabsSize"));
		if (column)
		{
			column.EnableWidthOverride(true);
			column.SetWidthOverride(AROR_COLUMN_WIDTH);
		}

		// Pinned to the top, level with the first row of items (below the title bar), so expanding grows it downwards
		AlignableSlot.SetVerticalAlign(list, LayoutVerticalAlign.Top);
		AlignableSlot.SetPadding(list, 0, AROR_TOP_OFFSET, AROR_COLUMN_GAP, 0);

		Widget old = list.GetChildren();
		while (old)
		{
			Widget next = old.GetSibling();
			old.RemoveFromHierarchy();
			old = next;
		}

		m_aARORButtons.Clear();
		m_aARORButtonCategory.Clear();
		m_aARORButtonType.Clear();

		if (!m_ARORTabHandler)
			m_ARORTabHandler = new AROR_ArsenalTabHandler(this);

		foreach (int category : m_aARORCategories)
		{
			bool selected = category == m_iARORCategory;
			AROR_AddButton(list, AROR_ArsenalCatalog.GetCategoryName(category), category, AROR_CATEGORY_BUTTON, selected, false);

			if (!selected)
				continue;

			foreach (int type : m_aARORTypes)
				AROR_AddButton(list, AROR_ArsenalCatalog.GetTypeName(category, type), category, type, type == m_iARORType, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void AROR_AddButton(notnull Widget list, string label, int category, int type, bool isSelected, bool isType)
	{
		Widget tab = GetGame().GetWorkspace().CreateWidgets(AROR_TAB_LAYOUT, list);
		if (!tab)
		{
			Print("[AROR] Arsenal tab layout failed to create: " + AROR_TAB_LAYOUT, LogLevel.WARNING);
			return;
		}

		float width = AROR_COLUMN_WIDTH;
		float height = AROR_CATEGORY_HEIGHT;
		float fontSize = AROR_CATEGORY_FONT;
		float indent;
		int fill = AROR_UIColors.SURFACE_RAISED;
		if (isType)
		{
			width = AROR_COLUMN_WIDTH - AROR_TYPE_INDENT;
			height = AROR_TYPE_HEIGHT;
			fontSize = AROR_TYPE_FONT;
			indent = AROR_TYPE_INDENT;
			fill = AROR_UIColors.SURFACE;
		}

		if (isSelected)
		{
			if (isType)
				fill = AROR_UIColors.LINE_STRONG;
			else
				fill = AROR_UIColors.REBEL_DIM;
		}

		// Overlay children and the overlay itself otherwise keep their natural size, centred
		array<string> stretchParts = {"Content", "Bg", "Label", "Button"};
		foreach (string stretchPart : stretchParts)
		{
			Widget stretched = tab.FindAnyWidget(stretchPart);
			if (!stretched)
				continue;

			AlignableSlot.SetHorizontalAlign(stretched, LayoutHorizontalAlign.Stretch);
			AlignableSlot.SetVerticalAlign(stretched, LayoutVerticalAlign.Stretch);
		}

		SizeLayoutWidget size = SizeLayoutWidget.Cast(tab);
		if (size)
		{
			size.EnableWidthOverride(true);
			size.EnableHeightOverride(true);
			size.SetWidthOverride(width);
			size.SetHeightOverride(height);
		}

		LayoutSlot.SetPadding(tab, indent, 0, 0, AROR_ROW_GAP);

		Widget background = tab.FindAnyWidget("Bg");
		if (background)
			background.SetColor(AROR_UIColors.Get(fill));

		RichTextWidget text = RichTextWidget.Cast(tab.FindAnyWidget("Label"));
		if (text)
		{
			// Roughly centres the line vertically in the button
			LayoutSlot.SetPadding(text, 10, (height - fontSize) * 0.5 - 1, 0, 0);
			text.SetText(label);
			text.SetExactFontSize(fontSize);
			if (isSelected || !isType)
				text.SetColor(AROR_UIColors.Get(AROR_UIColors.TEXT));
			else
				text.SetColor(AROR_UIColors.Get(AROR_UIColors.TEXT_MUTED));
		}

		Widget button = tab.FindAnyWidget("Button");
		if (!button)
			return;

		button.SetColor(Color.FromRGBA(0, 0, 0, 0));
		button.AddHandler(m_ARORTabHandler);
		m_aARORButtons.Insert(button);
		m_aARORButtonCategory.Insert(category);
		m_aARORButtonType.Insert(type);
	}

	//------------------------------------------------------------------------------------------------
	void AROR_OnTabClick(Widget w)
	{
		int index = m_aARORButtons.Find(w);
		if (index < 0)
			return;

		int category = m_aARORButtonCategory[index];
		int type = m_aARORButtonType[index];
		if (type == AROR_CATEGORY_BUTTON)
		{
			m_iARORCategory = category;
			m_iARORType = AROR_ArsenalCatalog.TYPE_ALL;
		}
		else
		{
			m_iARORType = type;
		}

		s_iARORLastCategory = m_iARORCategory;
		s_iARORLastType = m_iARORType;
		m_iLastShownPage = 0;

		// Refresh rebuilds the column holding the clicked button, so let the click finish first
		GetGame().GetCallqueue().CallLater(Refresh, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Lists only the selected category/type of a Resistance arsenal
	override protected void GetAllItems(out notnull array<IEntity> pItemsInStorage, BaseInventoryStorageComponent pStorage = null)
	{
		// The base class is also asked with the storage itself (and lists everything), so filter on both paths
		if (!AROR_IsResistance())
		{
			super.GetAllItems(pItemsInStorage, pStorage);
			return;
		}

		array<ResourceName> prefabs = {};
		AROR_GetStockedPrefabs(prefabs);
		AROR_PrepareTabs(prefabs);

		ChimeraWorld chimeraWorld = GetGame().GetWorld();
		ItemPreviewManagerEntity itemPreviewManagerEntity = chimeraWorld.GetItemPreviewManager();

		foreach (ResourceName prefab : prefabs)
		{
			if (m_ARORCatalog.Matches(prefab, m_iARORCategory, m_iARORType))
				pItemsInStorage.Insert(itemPreviewManagerEntity.ResolvePreviewEntityForPrefab(prefab));
		}
	}
}
