#include "VirtualItemMgr.h"
#include "Errors.h" // ASSERT macro
#include "SharedDefines.h" // item quality enum
#include <algorithm>
#include <cstdlib>

VirtualModifier::PremadeStatGroup const VirtualModifier::premadeStatGroups;

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
        return 1.0f / 7.0f;
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

VirtualItemMgr::VirtualItemMgr()
{
    WriteGuard guard(lock);
    const uint32 block = std::floor((double)(maxEntry - minEntry) / MAX_ITEM_SUBCLASS_WEAPON + 1);

    armorGenerator = EntryGenerator(minEntry, minEntry + block - 1);

    for (size_t i = 0; i < MAX_ITEM_SUBCLASS_WEAPON; ++i)
    {
        uint32 min = minEntry + block + (i*block);
        weaponGenerator[i] = EntryGenerator(min, min + block - 1);
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

VirtualItemTemplate* VirtualItemMgr::GenerateVirtualTemplate(ItemTemplate const* base, MemoryBind binding, VirtualModifier const& modifier)
{
    ASSERT(base);

    VirtualItemTemplate* temp = new VirtualItemTemplate(base, binding);
    GenerateStats(temp, modifier);

    WriteGuard guard(lock);
    uint32 entry = Generator(temp).GenerateEntry(store);
    temp->ItemId = entry;
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
    uint32 statscount = quality;
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

    std::vector<ItemModType> selectedStats;
    std::vector<int16> distributedPool;
    if (!modifier.statgroup.empty())
    {
        // select stats from preselected stat group
        for (uint32 i = 0; i < statscount; ++i)
            selectedStats.push_back(modifier.statgroup[urand(0, modifier.statgroup.size() - 1)]);

        // distribute pool to stats
        static const float mineachpct = 0.10f;
        ASSERT(mineachpct <= 1.0f / selectedStats.size() && mineachpct >= 0.0);

        // calculate min amount and take that from the randomly distributed pool
        int16 min_amount = std::floor(pool*mineachpct);
        int16 workpool = pool - selectedStats.size()*min_amount;

        // pick random positions from the workpool and use them to divide it into N random size parts
        // then add those to distributedPool along with the minimum amounts
        std::vector<int16> fences;
        fences.push_back(0);
        for (uint32 i = 1; i < selectedStats.size(); ++i)
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

    // apply other item data
    output->Quality = quality;
    output->StatsCount = statscount; // remember to modify in stat generation if two same stats are picked
    output->ItemLevel = ilevel;
    output->Armor = armor;
}

void VirtualItemMgr::SetVirtualTemplateMemoryBind(uint32 entry, MemoryBind binding)
{
    if (entry < minEntry || entry >= maxEntry)
        return;
    WriteGuard guard(lock);
    auto it = store.find(entry);
    if (it == store.end())
        return;
    it->second->memoryBinding = binding;
}

// does not directly remove the item. Removing delayed until actual delete
void VirtualItemMgr::Remove(uint32 entry)
{
    if (entry < minEntry || entry >= maxEntry)
        return;
    WriteGuard guard(lock);
    freed_entries.push_back(entry);
}

bool VirtualItemMgr::HasSpaceFor(uint32 amount)
{
    ReadGuard guard(lock);
    if (!armorGenerator.CanAllocate(amount, store))
        return false;

    for (size_t i = 0; i < MAX_ITEM_SUBCLASS_WEAPON; ++i)
    {
        if (!weaponGenerator[i].CanAllocate(amount, store))
            return false;
    }

    return true;
}

// not thread safe
void VirtualItemMgr::RemoveExcludedItemBound(std::set<uint32> not_removed_entries)
{
    for (auto entry : store)
    {
        if (entry.second->memoryBinding != BIND_ITEM)
            continue;
        if (not_removed_entries.find(entry.first) == not_removed_entries.end())
            Remove(entry.first);
    }

    ClearFreedEntries();
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

bool VirtualItemMgr::EntryGenerator::CanAllocate(uint32 amount, VirtualItemMgr::Store const& store) const
{
    uint32 start = nextEntry;
    uint32 current = start;
    for (uint32 u = 0; u < amount; ++u)
    {
        for (++current; true; ++current)
        {
            if (current >= maxEntry)
                current = minEntry;
            if (current == start)
                return false;
            if (store.find(current) == store.end())
                break;
        }
    }
    return true;
}

VirtualItemMgr::EntryGenerator& VirtualItemMgr::Generator(ItemTemplate* temp)
{
    if (temp->Class == ITEM_CLASS_ARMOR)
        return armorGenerator;
    ASSERT(temp->Class == ITEM_CLASS_WEAPON);
    return weaponGenerator[temp->SubClass];
}

// used to insert virtual items on load, not thread safe
bool VirtualItemMgr::InsertEntry(VirtualItemTemplate* virtualItem)
{
    ASSERT(virtualItem);
    uint32 entry = virtualItem->ItemId;
    if (entry < minEntry || entry >= maxEntry || store.find(entry) != store.end())
    {
        delete virtualItem;
        return false;
    }
    store[entry] = virtualItem;
    if (entry == Generator(virtualItem).PeekNext())
        Generator(virtualItem).GenerateEntry(store);
    return true;
}

// used to free item entries, not thread safe
void VirtualItemMgr::ClearFreedEntries()
{
    WriteGuard guard(lock);
    for (uint32 entry : freed_entries)
    {
        auto it = store.find(entry);
        if (it == store.end())
            continue;
        delete it->second;
        store.erase(entry);
    }
    freed_entries.clear();
}

VirtualModifier::PremadeStatGroup::PremadeStatGroup()
{
    premade[STAT_GROUP_WARRIOR] = {
        ITEM_MOD_HEALTH,
        ITEM_MOD_AGILITY,
        ITEM_MOD_STRENGTH
    };
    premade[STAT_GROUP_CASTER] = {
        ITEM_MOD_HEALTH,
        ITEM_MOD_MANA,
        ITEM_MOD_SPIRIT,
        ITEM_MOD_INTELLECT
    };
}

std::vector<ItemModType> const & VirtualModifier::PremadeStatGroup::Get(StatGroup group) const
{
    if (group == STAT_GROUP_RANDOM)
        group = static_cast<StatGroup>(urand(0, STAT_GROUP_COUNT - 1));
    ASSERT(group < STAT_GROUP_COUNT);

    return premade[group];
}
