from dataclasses import dataclass

from Options import Toggle, Choice, FreeText, PerGameCommonOptions, NamedRange, DeathLink, Visibility, Range


class Goal(Choice):
    """Sets the completion goal. This is the kingdom you must get the last story multi moon in to win the game.
    Valid options: Metro (A Traditional Festival), Luncheon (Cookatiel Showdown), Moon (Beat the game), Dark (Arrival at Rabbit Ridge), Darker (A Long Journey's End)"""
    display_name = "Goal"
    option_sand = 2
    option_lake = 4
    option_metro = 7
    option_luncheon = 10
    option_moon = 13
    option_dark = 15
    option_darker = 16
    default = "moon"  # default to moon

class StorySanity(Choice):
    """Adds story progression moons to the pool."""
    display_name = "Randomize Story Moons"
    option_single_moons = 1
    option_multi_moons = 2
    option_all = 3
    option_off = 0
    default = "off" # default to off

class ShopSanity(Choice):
    """Adds various shop items to the pool.
    shuffle: shuffles outfits amongst themselves keeping them in your game."""
    display_name = "Randomize Shops"
    option_shuffle = 1
    option_outfits  = 2
    option_non_outfits = 3
    option_all = 4
    option_off = 0
    default = "off"  # default to off

class RandomizeMoonColors(Choice):
    """Changes the color of moons.
    kingdom : shuffles color dependent on kingdom (all moons in one kingdom will be the same color similar to the original implementation)
    item : color is dependent on the item at the location (SMO items only)
    classification : color is dependent on the item classification of the item at the location
    random : randomizes the colors instead of using the default set.
    chaos : colors are completely random for each moon individually."""
    display_name = "Randomize Moon Colors"
    option_off = 0
    option_kingdom_random = 1
    option_item = 2
    option_classification = 3
    option_item_random = 4
    option_classification_random = 5
    option_chaos = 6
    default = "off"
    #visibility = 0b1101

class RandomizeMoonCount(Choice):
    """Randomizes each kingdom's moon count.
    same total: Moon counts still add up to 124 like in the base game.
    lock ruined: Moon counts still add up to 124, but ruined kingdom is always a 3 moon requirement.
    moderate: Up to +25% and down to -20% of normal per kingdom counts.
    extreme: Up to 200% of normal count.
    custom_weighted: Moon requirements can be customized (see following options)
    """
    display_name = "Randomize Moon Requirements"
    #visibility = 0b1101
    option_same_total = 1
    option_same_total_lock_ruined = 2
    option_moderate = 3
    option_extreme = 4
    option_custom_weighted = 5
    option_off = 0
    default = 5

class RequiredMoonTotal(Range):
    """The total number of moons required to unlock your goal kingdom
    Max is 816, as all 61 toadette moons are not randomized, and the darker side multi can't be reached without first unlocking the kingdom
    Must be at *least* equal to the number of kingdoms with moon requirements in your chosen goal, and at *most* equal to the number of moons possible before reaching your chosen goal kingdom. Will throw an OptionError if the required number of moons is invalid
    Requires "Randomize Moon Requirements" to be set to "Custom Weighted"
    """
    display_name = "Required Moon Total"
    range_start = 1
    range_end = 816
    default = 124

class RequiredMoonWeighting(Range):
    """Applies a weight for the number of moons required for each kingdom based on how far in the kingdom is
    At 100: Your first kingdom will require very few moons, and your final kingdom require many
    At -100: Your first kingdom will require a high number of moons, and your final kingdom will reuqire very few
    Negative values are **not** recommended
    Does very little if "Similar Moon Count Requirement Ratios" is enabled
    Requires "Randomize Moon Requirements" to be set to "Custom Weighted"
    """
    display_name = 'Required Moon Weighting'
    range_start = -100
    range_end = 100
    default = 30

class SimilarMoonRequirementRatios(Toggle):
    """If enabled, moon count requirements for each kingdom will be scaled according to RequiredMoonTotal, then randomized within +/- 5 of that scaled value
    Requires "Randomize Moon Requirements" to be set to "Custom Weighted"
    """
    display_name = 'Similar Moon Count Requirement Ratios'
    default = False

class LockRuined(Toggle):
    """If enabled, Ruined Kingdom will always require 3 moons
    Requires "Randomize Moon Requirements" to be set to "Custom Weighted"
    """
    display_name = 'Lock Ruined'
    default = False

class MoonDistribution(Choice):
    """
    Determines how much jitter is added to per-moon sorting.
    Lower values strictly force moons for early kingdom to appear early in logic
    Higher values allow moons from later kingdoms to apear earlierin logic
    """
    display_name = "Progression Guide Jitter"
    option_high = 2.5
    option_normal = 1.5
    option_low = 0.5
    option_none = 0
    default = "normal"

class CaptureSanity(Toggle):
    """Randomizes Captures.
    Warning this is an experimental feature!
    NOT ALL Captures have logic, use at YOU OWN RISK!
    Logic for post game is still missing.
    """
    display_name = "Randomize Captures"
    #visibility = 0b1101

class CommonCaptureSkips(Toggle):
    """
    Enables logic for frequently used capture skips
    Adds the following tricks:
    Frogless
    First Moon Skip
    Impossible Jump
    Red Maze Skip
    """
    display_name = "Common Capture Skips"
    default = False

class ExtraMoons(NamedRange):
    """
    Sets the percentage of extra moons available in the pool for each kingdom.
    Default: 175%
    """
    display_name = "Extra Moons"
    range_start = 100
    range_end = 350

    special_range_names = {
        "none": 100,
        "some": 125,
        "more": 150,
        "many": 175,
        "double": 200,
        "default": 200,
        "extra": 250,
        "all": 10000, # Set to 100x to guarantee every possible moon is spawned in
    }

    default = "double"

class ProgressiveMoons(Toggle):
    """
    When enabled, all moons will become generic
    Significantly reduces BK potential and forces kingdom to unlock one at a time
    """
    display_name = "Progressive Moons"
    default = False
    visibility = Visibility.none

class TrickJumpLogic(Choice):
    """
        Difficulty of trick jumps considered as in logic.
    """
    option_off = 0
    option_easy = 1
    option_intermediate = 2
    option_hard = 3

    default = "off"  # default to off

class MiscTrickLogic(Choice):
    """
        Difficulty of clips and glitches considered as in logic.
    """
    option_off = 0
    option_easy = 1
    option_intermediate = 2
    option_hard = 3

    default = "off"  # default to off

class SMODeathLink(DeathLink):
    __doc__ = DeathLink.__doc__ + "\n    In Super Mario Odyssey, Mario dying in any way sends a death and receiving a death causes Mario to die where he stands."

@dataclass
class SMOOptions(PerGameCommonOptions):
    goal: Goal
    story : StorySanity
    progressive_moons : ProgressiveMoons
    extra_moons : ExtraMoons
    moon_distribution : MoonDistribution
    shop_sanity : ShopSanity
    capture_sanity : CaptureSanity
    common_capture_skips : CommonCaptureSkips
    # replace: ReplaceUnneededMoons
    colors : RandomizeMoonColors
    counts : RandomizeMoonCount
    required_moon_total : RequiredMoonTotal
    required_moon_weighting : RequiredMoonWeighting
    similar_moon_ratios : SimilarMoonRequirementRatios
    lock_ruined : LockRuined
    death_link : SMODeathLink

