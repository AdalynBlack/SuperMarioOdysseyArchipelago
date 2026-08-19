from BaseClasses import CollectionState
from.Items import outfits, shop_items, moon_types

def count_moons(self, state: CollectionState, kingdom : str, player: int) -> int:
    """ Counts the number of in logic moons available for a given kingdom.
        Args:
            self: SMOWorld object for this player's world.
            state: The CollectionState of the current player.
            kingdom: A string containing the kingdom name.
            player: The index of this world's player.
        Return:
            Count of the moons for Kingdom 'kingdom'
    """
    amt = 0
    player_prog_items = state.prog_items[player]
    # for item_name in self.multiworld.worlds[player].item_name_groups[kingdom]:
    #     if state.has(item_name, player):
    #         amt += player_prog_items[item_name] if "Multi-Moon" not in item_name else 3
    amt += 0 if not kingdom + " Power Moon" in player_prog_items else player_prog_items[kingdom + " Power Moon"]
    amt += 0 if not kingdom + " Story Moon" in player_prog_items else player_prog_items[kingdom + " Story Moon"]
    amt += 0 if not kingdom + " Multi-Moon" in player_prog_items else player_prog_items[kingdom + " Multi-Moon"] * 3

    return amt


def total_moons(self, state: CollectionState, player: int) -> int:
    """Returns the cumulative count of items from an item group present in state.
        Args:
            self: SMOWorld object for this player's world.
            state: The CollectionState of the current player.
            player: The index of this world's player.
        Return:
            The number of total in logic power moons.
    """
    amt = 0

    for item_name in moon_types:
        if "Multi-Moon" in item_name:
            amt += state.count(item_name, player) * 3
        else:
            amt += state.count(item_name, player)

    return amt


