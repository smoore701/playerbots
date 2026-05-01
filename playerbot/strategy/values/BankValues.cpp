
#include "playerbot/playerbot.h"
#include "BankValues.h"
#include "ItemCountValue.h"
#include "Guilds/Guild.h"
#include "Guilds/GuildMgr.h"

using namespace ai;

bool ShouldBankDepositValue::Calculate()
{
    if (!ai->HasStrategy("rpg vendor", BotState::BOT_STATE_NON_COMBAT))
        return false;

    // Check if we have items worth banking
    uint32 bankEquipCount = AI_VALUE2(uint32, "item count", "usage " + std::to_string((uint8)ItemUsage::ITEM_USAGE_BANK_EQUIP));
    uint32 bankCraftCount = AI_VALUE2(uint32, "item count", "usage " + std::to_string((uint8)ItemUsage::ITEM_USAGE_BANK_CRAFT));

    if (bankEquipCount == 0 && bankCraftCount == 0)
        return false;

    // Check if bank has free space (approximate check)
    uint32 bankItemCount = 0;
    for (int i = BANK_SLOT_ITEM_START; i < BANK_SLOT_ITEM_END; ++i)
    {
        if (bot->GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            bankItemCount++;
    }

    if (bankItemCount >= (BANK_SLOT_ITEM_END - BANK_SLOT_ITEM_START))
        return false;

    return true;
}

bool ShouldBankWithdrawValue::Calculate()
{
    if (!ai->HasStrategy("rpg vendor", BotState::BOT_STATE_NON_COMBAT))
        return false;

    if (AI_VALUE(uint8, "bag space") > 80)
        return false;

    // Check bank for items that are now equippable
    for (int i = BANK_SLOT_ITEM_START; i < BANK_SLOT_ITEM_END; ++i)
    {
        Item* pItem = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (!pItem)
            continue;

        ItemPrototype const* proto = pItem->GetProto();
        if (!proto)
            continue;

        // Check if item is now equippable (was banked because too high level)
        if (proto->InventoryType != INVTYPE_NON_EQUIP && proto->RequiredLevel <= bot->GetLevel()
            && bot->CanUseItem(proto) == EQUIP_ERR_OK)
        {
            ItemQualifier qualifier(pItem);
            ItemUsage equipUsage = ItemUsageValue::QueryItemUsageForEquip(qualifier, bot);
            if (equipUsage == ItemUsage::ITEM_USAGE_EQUIP || equipUsage == ItemUsage::ITEM_USAGE_BAD_EQUIP)
                return true;
        }

        // Check if item is now needed for crafting
        if (proto->Class == ITEM_CLASS_TRADE_GOODS || proto->Class == ITEM_CLASS_MISC || proto->Class == ITEM_CLASS_REAGENT)
        {
            ItemQualifier qualifier(pItem);
            std::string qualStr = qualifier.GetQualifier();
            ItemUsage usage = AI_VALUE2(ItemUsage, "item usage", qualStr);
            if (usage == ItemUsage::ITEM_USAGE_SKILL)
                return true;
        }
    }

    // Also check bank bags
    for (int bag = BANK_SLOT_BAG_START; bag < BANK_SLOT_BAG_END; ++bag)
    {
        Bag* pBag = (Bag*)bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
        if (!pBag)
            continue;

        for (uint32 slot = 0; slot < pBag->GetBagSize(); ++slot)
        {
            Item* pItem = pBag->GetItemByPos(slot);
            if (!pItem)
                continue;

            ItemPrototype const* proto = pItem->GetProto();
            if (!proto)
                continue;

            if (proto->InventoryType != INVTYPE_NON_EQUIP && proto->RequiredLevel <= bot->GetLevel()
                && bot->CanUseItem(proto) == EQUIP_ERR_OK)
            {
                ItemQualifier qualifier(pItem);
                ItemUsage equipUsage = ItemUsageValue::QueryItemUsageForEquip(qualifier, bot);
                if (equipUsage == ItemUsage::ITEM_USAGE_EQUIP || equipUsage == ItemUsage::ITEM_USAGE_BAD_EQUIP)
                    return true;
            }

            if (proto->Class == ITEM_CLASS_TRADE_GOODS || proto->Class == ITEM_CLASS_MISC || proto->Class == ITEM_CLASS_REAGENT)
            {
                ItemQualifier qualifier(pItem);
                std::string qualStr = qualifier.GetQualifier();
                ItemUsage usage = AI_VALUE2(ItemUsage, "item usage", qualStr);
                if (usage == ItemUsage::ITEM_USAGE_SKILL)
                    return true;
            }
        }
    }

    return false;
}

#ifndef MANGOSBOT_ZERO
bool ShouldGuildBankDepositValue::Calculate()
{
    if (!bot->GetGuildId())
        return false;

    Guild* guild = sGuildMgr.GetGuildById(bot->GetGuildId());
    if (!guild)
        return false;

    if (guild->GetPurchasedTabs() == 0)
        return false;

    if (!guild->IsMemberHaveRights(bot->GetGUIDLow(), 0, GUILD_BANK_RIGHT_DEPOSIT_ITEM))
        return false;

    // Look for surplus crafting materials and consumables we can share
    std::list<Item*> items = AI_VALUE2(std::list<Item*>, "inventory items", "usage " + std::to_string((uint8)ItemUsage::ITEM_USAGE_VENDOR));
    for (auto item : items)
    {
        if (!item)
            continue;

        ItemPrototype const* proto = item->GetProto();
        if (!proto)
            continue;

        // Only deposit trade goods and reagents to guild bank
        if (proto->Class == ITEM_CLASS_TRADE_GOODS || proto->Class == ITEM_CLASS_REAGENT)
        {
            if (proto->Quality >= ITEM_QUALITY_NORMAL && proto->Bonding != BIND_WHEN_PICKED_UP)
                return true;
        }
    }

    // Also deposit items tagged for AH that are trade goods (guild members might need them)
    std::list<Item*> ahItems = AI_VALUE2(std::list<Item*>, "inventory items", "usage " + std::to_string((uint8)ItemUsage::ITEM_USAGE_AH));
    for (auto item : ahItems)
    {
        if (!item)
            continue;

        ItemPrototype const* proto = item->GetProto();
        if (!proto)
            continue;

        if (proto->Class == ITEM_CLASS_TRADE_GOODS || proto->Class == ITEM_CLASS_REAGENT)
        {
            if (proto->Quality >= ITEM_QUALITY_NORMAL && proto->Bonding != BIND_WHEN_PICKED_UP)
                return true;
        }
    }

    return false;
}

bool ShouldGuildBankWithdrawValue::Calculate()
{
    if (!bot->GetGuildId())
        return false;

    if (AI_VALUE(uint8, "bag space") > 80)
        return false;

    Guild* guild = sGuildMgr.GetGuildById(bot->GetGuildId());
    if (!guild)
        return false;

    if (guild->GetPurchasedTabs() == 0)
        return false;

    // Check if bot has rights to view/withdraw from tab 0
    if (!guild->IsMemberHaveRights(bot->GetGUIDLow(), 0, GUILD_BANK_RIGHT_VIEW_TAB))
        return false;

    // Check guild bank for items the bot can use
    for (uint8 tabId = 0; tabId < guild->GetPurchasedTabs(); ++tabId)
    {
        if (!guild->IsMemberHaveRights(bot->GetGUIDLow(), tabId, GUILD_BANK_RIGHT_VIEW_TAB))
            continue;

        for (uint8 slotId = 0; slotId < GUILD_BANK_MAX_SLOTS; ++slotId)
        {
            Item* item = guild->GetItem(tabId, slotId);
            if (!item)
                continue;

            ItemPrototype const* proto = item->GetProto();
            if (!proto)
                continue;

            // Check if we can equip this item
            if (proto->InventoryType != INVTYPE_NON_EQUIP && bot->CanUseItem(proto) == EQUIP_ERR_OK)
            {
                ItemQualifier qualifier(item);
                ItemUsage equipUsage = ItemUsageValue::QueryItemUsageForEquip(qualifier, bot);
                if (equipUsage == ItemUsage::ITEM_USAGE_EQUIP)
                    return true;
            }

            // Check if we need this for crafting
            if (proto->Class == ITEM_CLASS_TRADE_GOODS || proto->Class == ITEM_CLASS_REAGENT)
            {
                ItemQualifier qualifier(item);
                std::string qualStr = qualifier.GetQualifier();
                ItemUsage usage = AI_VALUE2(ItemUsage, "item usage", qualStr);
                if (usage == ItemUsage::ITEM_USAGE_SKILL)
                    return true;
            }
        }
    }

    return false;
}
#endif
