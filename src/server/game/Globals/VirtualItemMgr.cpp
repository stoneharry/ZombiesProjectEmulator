#include "VirtualItemMgr.h"
#include "Errors.h" // ASSERT macro
#include "SharedDefines.h" // item quality enum
#include <algorithm>
#include <cstdlib>

VirtualModifier::StatGroupData const VirtualModifier::premadeStatGroupData;

const std::vector<ItemSubclassArmor> VirtualItemMgr::armorSubclasses = {
    ITEM_SUBCLASS_ARMOR_MISC,
    ITEM_SUBCLASS_ARMOR_CLOTH,
    ITEM_SUBCLASS_ARMOR_LEATHER,
    ITEM_SUBCLASS_ARMOR_MAIL,
    ITEM_SUBCLASS_ARMOR_PLATE,
    //ITEM_SUBCLASS_ARMOR_BUCKLER,
    ITEM_SUBCLASS_ARMOR_SHIELD,
    //ITEM_SUBCLASS_ARMOR_LIBRAM,
    //ITEM_SUBCLASS_ARMOR_IDOL,
    //ITEM_SUBCLASS_ARMOR_TOTEM,
    //ITEM_SUBCLASS_ARMOR_SIGIL,
};
const std::vector<ItemSubclassWeapon> VirtualItemMgr::weaponSubclasses = {
    ITEM_SUBCLASS_WEAPON_AXE,
    ITEM_SUBCLASS_WEAPON_AXE2,
    ITEM_SUBCLASS_WEAPON_BOW,
    ITEM_SUBCLASS_WEAPON_GUN,
    ITEM_SUBCLASS_WEAPON_MACE,
    ITEM_SUBCLASS_WEAPON_MACE2,
    ITEM_SUBCLASS_WEAPON_POLEARM,
    ITEM_SUBCLASS_WEAPON_SWORD,
    ITEM_SUBCLASS_WEAPON_SWORD2,
    //ITEM_SUBCLASS_WEAPON_obsolete,
    ITEM_SUBCLASS_WEAPON_STAFF,
    //ITEM_SUBCLASS_WEAPON_EXOTIC,
    //ITEM_SUBCLASS_WEAPON_EXOTIC2,
    ITEM_SUBCLASS_WEAPON_FIST,
    //ITEM_SUBCLASS_WEAPON_MISC,
    ITEM_SUBCLASS_WEAPON_DAGGER,
    //ITEM_SUBCLASS_WEAPON_THROWN,
    //ITEM_SUBCLASS_WEAPON_SPEAR,
    ITEM_SUBCLASS_WEAPON_CROSSBOW,
    //ITEM_SUBCLASS_WEAPON_WAND,
    //ITEM_SUBCLASS_WEAPON_FISHING_POLE,
};

void VirtualItemTemplate::UpdateDisplay()
{
    // Get the correct display ID
    if (ItemEntry const* dbcitem = sItemStore.LookupEntry(ItemId))
        DisplayInfoID = dbcitem->DisplayId;
}

VirtualModifier::VirtualModifier() : ilevel(0), quality(MAX_ITEM_QUALITY), statpool(-1), statgroup(STAT_GROUP_RANDOM)
{
}

float VirtualModifier::GetSlotStatModifier(InventoryType invtype)
{
    switch (invtype)
    {
        case INVTYPE_HEAD:
        case INVTYPE_CHEST:
        case INVTYPE_ROBE:
        case INVTYPE_LEGS:
        case INVTYPE_2HWEAPON:
            return 1.0f;

        case INVTYPE_SHOULDERS:
        case INVTYPE_HANDS:
        case INVTYPE_WAIST:
        case INVTYPE_FEET:
            return 0.75f;

        case INVTYPE_WRISTS:
        case INVTYPE_NECK:
        case INVTYPE_CLOAK:
        case INVTYPE_FINGER:
        case INVTYPE_HOLDABLE:
        case INVTYPE_SHIELD:
            return 9.0f / 16.0f;

        case INVTYPE_WEAPON:
        case INVTYPE_WEAPONMAINHAND:
        case INVTYPE_WEAPONOFFHAND:
            return 27.0f / 64.0f;

        case INVTYPE_RANGED:
        case INVTYPE_RANGEDRIGHT:
        case INVTYPE_THROWN:
            return 81.0f / 256.0f;

        default:
            return 1.0f;
    }
    return 1.0f;
}

float VirtualModifier::GetStatRate(ItemModType stat)
{
    switch (stat)
    {
        case ITEM_MOD_RANGED_ATTACK_POWER:
            return 0.4f;
        case ITEM_MOD_ARMOR_PENETRATION_RATING:
            return 0.14f;
        case ITEM_MOD_ATTACK_POWER:
            return 0.5f;
        case ITEM_MOD_SPELL_HEALING_DONE:
            return 0.45f;
        case ITEM_MOD_MANA_REGENERATION:
            return 2.0f;
        case ITEM_MOD_HEALTH_REGEN:
            return 2.5f;
        case ITEM_MOD_SPELL_PENETRATION:
            return 0.8f;
        case ITEM_MOD_BLOCK_VALUE:
            return 0.65f;
        case ITEM_MOD_SPELL_POWER:
            return 0.86f;
        case ITEM_MOD_DEFENSE_SKILL_RATING:
            return 1.2f;
        default:
            return 1.0f;
    }
    return 1.0f;
}

VirtualItemMgr & VirtualItemMgr::instance()
{
    static VirtualItemMgr obj;
    return obj;
}

VirtualItemMgr::VirtualItemMgr()
{
    WriteGuard guard(lock);

    // Some basic code to divide the given entry range between all item types
    const uint32 block = std::floor((double)(maxEntry - minEntry) / (weaponSubclasses.size() + armorSubclasses.size()));
    size_t blockCounter = 0;

    for (auto subclass : armorSubclasses)
    {
        uint32 min = minEntry + (blockCounter*block);
        armorGenerator[subclass] = EntryGenerator(min, min + block - 1);
        ++blockCounter;
    }

    for (auto subclass : weaponSubclasses)
    {
        uint32 min = minEntry + (blockCounter*block);
        weaponGenerator[subclass] = EntryGenerator(min, min + block - 1);
        ++blockCounter;
    }
}

VirtualItemMgr::~VirtualItemMgr()
{
    WriteGuard guard(lock);
    for (auto it : store)
    {
        delete it.second;
    }
    store.clear();
}

VirtualItemTemplate const* VirtualItemMgr::GetVirtualTemplate(uint32 entry)
{
    if (entry < minEntry || entry >= maxEntry)
        return nullptr;
    ReadGuard guard(lock);
    auto it = store.find(entry);
    if (it != store.end())
        return it->second;
    return nullptr;
}

VirtualItemTemplate* VirtualItemMgr::GenerateVirtualTemplate(ItemTemplate const* base, VirtualModifier const& modifier)
{
    if (!base)
        return nullptr;

    VirtualItemTemplate* temp = new VirtualItemTemplate(base);
    GenerateStats(temp, modifier);

    WriteGuard guard(lock);
    EntryGenerator* generator = Generator(temp);
    if (!generator)
        return nullptr;

    uint32 entry = generator->GenerateEntry(store);
    temp->ItemId = entry;
    temp->UpdateDisplay();

    delete store[entry];
    store[entry] = temp;

    return temp;
}

void VirtualItemMgr::GenerateStats(ItemTemplate* output, VirtualModifier const& modifier) const
{
    // decide quality
    uint32 quality = output->Quality;
    if (modifier.quality < MAX_ITEM_QUALITY)
        quality = modifier.quality;
    else
    {
        quality = output->Quality;

        // these are not percentage chances. They represent areas of a number line made from their sum
        static const float chances[MAX_ITEM_QUALITY] = {
            0,      // poor
            200,    // normal
            100,    // uncommon
            30,     // rare
            15,     // epic
            0,     // legendary
            0,      // artifact
            0,      // heirloom
        };

        uint32 sum = 0;
        for (auto u : chances)
            sum += u;

        if (sum >= 1)
        {
            uint32 rand = urand(1, sum);
            sum = 0;
            for (size_t i = 0; i < MAX_ITEM_QUALITY; ++i)
            {
                sum += chances[i];
                if (sum < rand)
                    continue;

                quality = i;
                break;
            }
        }

        quality = std::max(output->Quality, quality); // dont generate quality below original
    }
    ASSERT(quality < MAX_ITEM_QUALITY);

    // decide stat amount
    uint32 statscount = quality - 1;
    if (statscount < 0)
        statscount = 0;
    ASSERT(statscount <= MAX_ITEM_PROTO_STATS);

    // decide itemlevel
    uint32 ilevel = output->ItemLevel;
    if (modifier.ilevel)
        ilevel = modifier.ilevel;
    else
        ilevel = output->ItemLevel + ((int32(quality) - int32(output->Quality)) * 5);

    // decide armor
    uint32 armor = 0;
    if (output->Class == ITEM_CLASS_ARMOR)
    {
        float divider = ((int32(output->Quality) - int32(ITEM_QUALITY_NORMAL)) / 10.0f) + 1.0f;
        float white = output->Armor / divider;
        float multiplier = ((int32(quality) - int32(ITEM_QUALITY_NORMAL)) / 10.0f) + 1.0f;
        armor = white * multiplier;
    }

    // clear old stats
    for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
    {
        output->ItemStat[i].ItemStatType = 0;
        output->ItemStat[i].ItemStatValue = 0;
    }

    // select stat pool
    int16 pool = 0;
    if (modifier.statpool == -1)
        pool = ilevel;
    else
        pool = modifier.statpool;
    ASSERT(pool >= 0 && pool < 0x7FFF);

    // select stat group
    StatGroup statgroupid = modifier.statgroup;
    if (modifier.statgroup == STAT_GROUP_RANDOM && output->Class == ITEM_CLASS_ARMOR)
    {
        std::vector<StatGroup> const& statgroups = modifier.premadeStatGroupData.GetArmorSubclassStatGroups((ItemSubclassArmor)output->SubClass);
        if (!statgroups.empty())
            statgroupid = statgroups[urand(0, statgroups.size() - 1)];
    }
    if (statgroupid == STAT_GROUP_RANDOM)
        statgroupid = static_cast<StatGroup>(urand(0, STAT_GROUP_COUNT - 1));
    ASSERT(statgroupid < STAT_GROUP_COUNT); // must not be random anymore
    std::vector<ItemModType> const& statgroup = modifier.premadeStatGroupData.GetStatGroupStats(statgroupid);

    std::vector<ItemModType> selectedStats;
    std::vector<int16> distributedPool;
    if (statscount && !statgroup.empty())
    {
        // select stats from preselected stat group
        for (uint32 i = 0; i < statscount; ++i)
            selectedStats.push_back(statgroup[urand(0, statgroup.size() - 1)]);

        // distribute pool to stats
        const float mineachpct = 0.5f / selectedStats.size();
        ASSERT(mineachpct <= 1.0f / selectedStats.size() && mineachpct >= 0.0);

        // calculate min amount and take that from the randomly distributed pool
        int16 min_amount = std::floor(pool*mineachpct);
        int16 workpool = pool - selectedStats.size()*min_amount;

        // pick random positions from the workpool and use them to divide it into N random size parts
        // then add those to distributedPool along with the minimum amounts
        std::vector<int16> fences;
        fences.push_back(0);
        for (int32 i = 1; i < int32(selectedStats.size()); ++i)
            fences.push_back(urand(0, workpool));
        fences.push_back(workpool);
        std::sort(fences.begin(), fences.end());
        for (int32 i = 1; i < int32(fences.size()); ++i)
            distributedPool.push_back(min_amount + fences[i] - fences[i - 1]);
    }

    // apply new stats
    distributedPool.resize(selectedStats.size()); // ensure counts match
    uint32 setStats = 0;
    for (size_t i = 0; i < std::min(selectedStats.size(), size_t(MAX_ITEM_PROTO_STATS)); ++i)
    {
        for (uint32 j = 0; j < MAX_ITEM_PROTO_STATS; ++j)
        {
            // if we are at a free stat slot or we are at a stat slot that has the same stat type
            if (j >= setStats || output->ItemStat[j].ItemStatType == selectedStats[i])
            {
                uint32 finalStatValue = std::floor(distributedPool[i] / VirtualModifier::GetStatRate(selectedStats[i]) * VirtualModifier::GetSlotStatModifier((InventoryType)output->InventoryType));
                output->ItemStat[j].ItemStatType = selectedStats[i];
                output->ItemStat[j].ItemStatValue += finalStatValue;
                setStats = std::max(setStats, uint32(j + 1));
                break;
            }
        }
    }
    statscount = setStats;

    // set socket colors
    std::vector<SocketColor> const& socketcolors = modifier.premadeStatGroupData.GetStatGroupSockets(statgroupid);
    if (!socketcolors.empty())
    {
        for (int32 i = 0; i < MAX_ITEM_PROTO_SOCKETS; ++i)
        {
            if (!output->Socket[i].Color)
                continue;
            if (output->Socket[i].Color != SOCKET_COLOR_RED &&
                output->Socket[i].Color != SOCKET_COLOR_BLUE &&
                output->Socket[i].Color != SOCKET_COLOR_YELLOW)
                continue;
            output->Socket[i].Color = socketcolors[urand(0, socketcolors.size() - 1)];
        }
    }

    // apply other item data
    output->Quality = quality;
    output->StatsCount = statscount; // remember to modify in stat generation if two same stats are picked
    output->ItemLevel = ilevel;
    output->Armor = armor;
}

bool VirtualItemMgr::IsVirtualTemplate(ItemTemplate const * base)
{
    if (!((base->FlagsCu & ITEM_FLAGS_CU_VIRTUAL_ITEM_BASE) != 0 &&
        (base->Class == ITEM_CLASS_WEAPON || base->Class == ITEM_CLASS_ARMOR) &&
        base->RandomProperty == 0 && base->RandomSuffix == 0 &&
        base->ScalingStatDistribution == 0 && base->ScalingStatValue == 0))
        return false;

    if (base->Class == ITEM_CLASS_ARMOR)
    {
        for (auto subclass : armorSubclasses)
            if (base->SubClass == subclass)
                return true;
    }

    if (base->Class == ITEM_CLASS_WEAPON)
    {
        for (auto subclass : weaponSubclasses)
            if (base->SubClass == subclass)
                return true;
    }
    return false;
}

uint32 VirtualItemMgr::EntryGenerator::GenerateEntry(VirtualItemMgr::Store const& store)
{
    uint32 entry = nextEntry;
    for (uint32 i = nextEntry + 1; true; ++i)
    {
        if (i >= maxEntry)
            i = minEntry;
        if (i == nextEntry)
            break;
        if (store.find(i) == store.end())
        {
            nextEntry = i;
            break;
        }
    }

    ASSERT(entry != nextEntry);
    ASSERT(entry >= VirtualItemMgr::minEntry);
    ASSERT(entry <= VirtualItemMgr::maxEntry);
    return entry;
}

uint32 VirtualItemMgr::EntryGenerator::PeekNext() const
{
    return nextEntry;
}

VirtualItemMgr::EntryGenerator::EntryGenerator() : minEntry(0), maxEntry(1)
{
    ASSERT(minEntry < maxEntry);
    nextEntry = minEntry;
}

VirtualItemMgr::EntryGenerator::EntryGenerator(uint32 min, uint32 max) : minEntry(min), maxEntry(max)
{
    ASSERT(minEntry < maxEntry);
    nextEntry = minEntry;
}

VirtualItemMgr::EntryGenerator* VirtualItemMgr::Generator(ItemTemplate* temp)
{
    if (temp->Class == ITEM_CLASS_ARMOR)
    {
        auto it = armorGenerator.find((ItemSubclassArmor)temp->SubClass);
        if (it != armorGenerator.end())
            return &it->second;
    }
    if (temp->Class == ITEM_CLASS_WEAPON)
    {
        auto it = weaponGenerator.find((ItemSubclassWeapon)temp->SubClass);
        if (it != weaponGenerator.end())
            return &it->second;
    }
    return nullptr;
}

bool VirtualItemMgr::InsertEntry(VirtualItemTemplate* virtualItem)
{
    if (!virtualItem)
        return false;
    EntryGenerator* generator = Generator(virtualItem);
    if (!generator)
        return false;

    uint32 entry = virtualItem->ItemId;
    if (entry < minEntry || entry >= maxEntry || store.find(entry) != store.end())
        return false;
    store[entry] = virtualItem;

    if (entry == generator->PeekNext())
        generator->GenerateEntry(store);
    return true;
}

VirtualModifier::StatGroupData::StatGroupData()
{
    stat_group_stats[STAT_GROUP_HEALING] = {
        ITEM_MOD_STAMINA,
        ITEM_MOD_INTELLECT,
        ITEM_MOD_SPIRIT,
        ITEM_MOD_HASTE_SPELL_RATING,
        ITEM_MOD_CRIT_SPELL_RATING,
        ITEM_MOD_MANA_REGENERATION,
        ITEM_MOD_SPELL_POWER
    };
    stat_group_stats[STAT_GROUP_INT_DPS] = {
        ITEM_MOD_STAMINA,
        ITEM_MOD_INTELLECT,
        ITEM_MOD_HIT_SPELL_RATING,
        ITEM_MOD_HASTE_SPELL_RATING,
        ITEM_MOD_CRIT_SPELL_RATING,
        ITEM_MOD_MANA_REGENERATION,
        ITEM_MOD_SPELL_POWER,
        ITEM_MOD_SPELL_PENETRATION
    };
    stat_group_stats[STAT_GROUP_STR_DPS] = {
        ITEM_MOD_STAMINA,
        ITEM_MOD_STRENGTH,
        ITEM_MOD_HIT_MELEE_RATING,
        ITEM_MOD_CRIT_MELEE_RATING,
        ITEM_MOD_HASTE_MELEE_RATING,
        ITEM_MOD_EXPERTISE_RATING,
        ITEM_MOD_ATTACK_POWER,
        ITEM_MOD_ARMOR_PENETRATION_RATING
    };
    stat_group_stats[STAT_GROUP_STR_TANK] = {
        ITEM_MOD_STAMINA,
        ITEM_MOD_STRENGTH,
        ITEM_MOD_HEALTH,
        ITEM_MOD_DEFENSE_SKILL_RATING,
        ITEM_MOD_DODGE_RATING,
        ITEM_MOD_PARRY_RATING,
        ITEM_MOD_HIT_RATING,
        ITEM_MOD_HEALTH_REGEN
    };
    stat_group_stats[STAT_GROUP_AGI_DPS] = {
        ITEM_MOD_STAMINA,
        ITEM_MOD_AGILITY,
        ITEM_MOD_HIT_MELEE_RATING,
        ITEM_MOD_CRIT_MELEE_RATING,
        ITEM_MOD_HASTE_MELEE_RATING,
        ITEM_MOD_EXPERTISE_RATING,
        ITEM_MOD_ATTACK_POWER,
        ITEM_MOD_ARMOR_PENETRATION_RATING
    };
    stat_group_stats[STAT_GROUP_AGI_TANK] = {
        ITEM_MOD_STAMINA,
        ITEM_MOD_AGILITY,
        ITEM_MOD_HEALTH,
        ITEM_MOD_DEFENSE_SKILL_RATING,
        ITEM_MOD_DODGE_RATING,
        ITEM_MOD_PARRY_RATING,
        ITEM_MOD_HIT_RATING,
        ITEM_MOD_HEALTH_REGEN
    };
    stat_group_stats[STAT_GROUP_AGI_RANGED] = {
        ITEM_MOD_STAMINA,
        ITEM_MOD_AGILITY,
        ITEM_MOD_HIT_RANGED_RATING,
        ITEM_MOD_CRIT_RANGED_RATING,
        ITEM_MOD_HASTE_RANGED_RATING,
        ITEM_MOD_EXPERTISE_RATING,
        ITEM_MOD_RANGED_ATTACK_POWER,
        ITEM_MOD_ARMOR_PENETRATION_RATING
    };

    // socket groups
    stat_group_sockets[STAT_GROUP_HEALING] = {
        SOCKET_COLOR_BLUE
    };

    stat_group_sockets[STAT_GROUP_INT_DPS] = {
        SOCKET_COLOR_BLUE
    };

    stat_group_sockets[STAT_GROUP_STR_DPS] = {
        SOCKET_COLOR_YELLOW
    };

    stat_group_sockets[STAT_GROUP_STR_TANK] = {
        SOCKET_COLOR_RED
    };

    stat_group_sockets[STAT_GROUP_AGI_DPS] = {
        SOCKET_COLOR_YELLOW
    };

    stat_group_sockets[STAT_GROUP_AGI_TANK] = {
        SOCKET_COLOR_RED
    };

    stat_group_sockets[STAT_GROUP_AGI_RANGED] = {
        SOCKET_COLOR_YELLOW
    };

    // type stat groups
    armor_type_stat_groups[ITEM_SUBCLASS_ARMOR_CLOTH] = {
        STAT_GROUP_HEALING,
        STAT_GROUP_INT_DPS
    };

    armor_type_stat_groups[ITEM_SUBCLASS_ARMOR_LEATHER] = {
        STAT_GROUP_HEALING,
        STAT_GROUP_INT_DPS,
        STAT_GROUP_AGI_DPS,
        STAT_GROUP_AGI_TANK,
        STAT_GROUP_AGI_RANGED
    };

    armor_type_stat_groups[ITEM_SUBCLASS_ARMOR_MAIL] = {
        STAT_GROUP_HEALING,
        STAT_GROUP_STR_DPS,
        STAT_GROUP_STR_TANK,
        STAT_GROUP_AGI_DPS,
        STAT_GROUP_AGI_RANGED
    };

    armor_type_stat_groups[ITEM_SUBCLASS_ARMOR_PLATE] = {
        STAT_GROUP_HEALING,
        STAT_GROUP_INT_DPS,
        STAT_GROUP_STR_DPS,
        STAT_GROUP_STR_TANK
    };

}

std::vector<ItemModType> const & VirtualModifier::StatGroupData::GetStatGroupStats(StatGroup group) const
{
    if (group == STAT_GROUP_RANDOM)
        group = static_cast<StatGroup>(urand(0, STAT_GROUP_COUNT - 1));
    ASSERT(group < STAT_GROUP_COUNT);

    return stat_group_stats[group];
}

std::vector<SocketColor> const & VirtualModifier::StatGroupData::GetStatGroupSockets(StatGroup group) const
{
    if (group == STAT_GROUP_RANDOM)
        group = static_cast<StatGroup>(urand(0, STAT_GROUP_COUNT - 1));
    ASSERT(group < STAT_GROUP_COUNT);

    return stat_group_sockets[group];
}

std::vector<StatGroup> const & VirtualModifier::StatGroupData::GetArmorSubclassStatGroups(ItemSubclassArmor subclass) const
{
    return armor_type_stat_groups[subclass];
}
