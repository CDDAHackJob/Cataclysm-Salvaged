/** \page map_management Map Management
 *
 * Cataclysm DDA uses a shifting coordinate system. This means that when the player moves to a new
 * submap, the (0, 0) coordinate moves from one submap to another, and the coordinates of all entities
 * on the map are shifted to accommodate this.
 *
 * Furthermore, the map is split into a hierarchical structure. At the top-level there are "overmaps",
 * which are very large chunks of the gameworld. The overmap is split into overmap tiles. When you use 'm'
 * to view the map in-game, what you see are overmap tiles. Each overmap tile is then again split into
 * four submaps (2x2). See \ref overmap and \ref submap.
 *
 * Parallel to this, there is the \ref mapbuffer. The mapbuffer manages and stores all submaps. Note that
 * this is separate to the overmap, which stores overmap tiles. The mapbuffer simply loads all submaps that
 * have ever been created into memory and keeps them there in a list. It does not "sort" them into overmaps
 * or anything like that.
 *
 * Again parallel to this, there is \ref map and `g->m`. A map contains a 2D pointer-array of submaps, for instance
 * 11x11 submaps. These are usually centered around the player, which is the reason the coordinate system shifts.
 * For all these submaps, it also caches additional data, such as lighting, scent, etc. This is basically
 * where all the "on-screen" processing happens. Most of the source code modifies terrain, monsters and so on only by
 * creating a map instance and using its methods.
 *
 * \section related_classes Related Classes
 * \ref mapbuffer \ref map \ref submap
 */

/*! @page Effects_Stat_Strength
 *  @brief Cross referenced effects of the Strength stat.
 *  @par
 */
/*! @page Effects_Stat_Dexterity
 *  @brief Cross referenced effects of the Dexterity stat.
 *  @par
 */
/*! @page Effects_Stat_Intelligence
 *  @brief Cross referenced effects of the Intelligence stat.
 *  @par
 */
/*! @page Effects_Stat_Perception
 *  @brief Cross referenced effects of the Perception stat.
 *  @par
 */
/*! @page Effects_Skill_Barter
 *  @brief Cross referenced effects of the Barter skill.
 *  @par
 */
/*! @page Effects_Skill_Computer
 *  @brief Cross referenced effects of the Computer skill.
 *  @par
 */
/*! @page Effects_Skill_Carpentry
 *  @brief Cross referenced effects of the Carpentry skill.
 *  @par
 */
/*! @page Effects_Skill_Cooking
 *  @brief Cross referenced effects of the Cooking skill.
 *  @par
 */
/*! @page Effects_Skill_Driving
 *  @brief Cross referenced effects of the Driving skill.
 *  @par
 */
/*! @page Effects_Skill_Electronics
 *  @brief Cross referenced effects of the Electronics skill.
 *  @par
 */
/*! @page Effects_Skill_Fabrication
 *  @brief Cross referenced effects of the Fabrication skill.
 *  @par
 */
/*! @page Effects_Skill_Firstaid
 *  @brief Cross referenced effects of the Firstaid skill.
 *  @par
 */
/*! @page Effects_Skill_Mechanics
 *  @brief Cross referenced effects of the Mechanics skill.
 *  @par
 */
/*! @page Effects_Skill_Speech
 *  @brief Cross referenced effects of the Speech skill.
 *  @par
 */
/*! @page Effects_Skill_Survival
 *  @brief Cross referenced effects of the Survival skill.
 *  @par
 */
/*! @page Effects_Skill_Swimming
 *  @brief Cross referenced effects of the Swimming skill.
 *  @par
 */
/*! @page Effects_Skill_Tailor
 *  @brief Cross referenced effects of the Tailor skill.
 *  @par
 */
/*! @page Effects_Skill_Traps
 *  @brief Cross referenced effects of the Traps skill.
 *  @par
 */
/*! @page Effects_Skill_Archery
 *  @brief Cross referenced effects of the Archery skill.
 *  @par
 */
/*! @page Effects_Skill_Bashing
 *  @brief Cross referenced effects of the Bashing skill.
 *  @par
 */
/*! @page Effects_Skill_Cutting
 *  @brief Cross referenced effects of the Cutting skill.
 *  @par
 */
/*! @page Effects_Skill_Dodge
 *  @brief Cross referenced effects of the Dodge skill.
 *  @par
 */
/*! @page Effects_Skill_Gun
 *  @brief Cross referenced effects of the Gun skill.
 *  @par
 */
/*! @page Effects_Skill_Launcher
 *  @brief Cross referenced effects of the Launcher skill.
 *  @par
 */
/*! @page Effects_Skill_Melee
 *  @brief Cross referenced effects of the Melee skill.
 *  @par
 */
/*! @page Effects_Skill_Stabbing
 *  @brief Cross referenced effects of the Stabbing skill.
 *  @par
 */
/*! @page Effects_Skill_Throw
 *  @brief Cross referenced effects of the Throw skill.
 *  @par
 */
/*! @page Effects_Skill_Unarmed
 *  @brief Cross referenced effects of the Unarmed skill.
 *  @par
 */
/*! @page Effects_Skill_Pistol
 *  @brief Cross referenced effects of the Pistol skill.
 *  @par
 */
/*! @page Effects_Skill_Rifle
 *  @brief Cross referenced effects of the Rifle skill.
 *  @par
 */
/*! @page Effects_Skill_Shotgun
 *  @brief Cross referenced effects of the Shotgun skill.
 *  @par
 */
/*! @page Effects_Skill_Smg
 *  @brief Cross referenced effects of the Smg skill.
 *  @par
 */
/*! @page Effects_Skill_Lockpick
 *  @brief Cross referenced effects of the Lock picking skill.
 *  @par
 */
/*! @page Effects_Skill_Chemistry
 *  @brief Cross referenced effects of the Chemistry skill.
 *  @par
 */
/*! @page emovedata
 *  @brief Handles uploading data, calling for downloads of data, checking the contents of a memory card, and deletion of data from device or card.
 *  @par Emovedata gives a slightly flexible UI in two parts. First it checks if the device can upload or download data, and populates the initial menu.
 * The UI for downloading data is the same as before, opening up a window to select any number of memory cards (with the HAS_DATA flag) on or next to the player.
 * Once the selection is complete, the activity handler is called and does its thing.
 * The UI for uploading data is different in that it is made to select only one memory card to upload to, and gives the player options.
 * A flexible UI is made based on what data types the device holds; the first option is a simple "upload everything," and the rest are for specific data types.
 * The "check" function works to select a single memory card, and returns a popup listing a summary of the contents.
 * For simpler data types it will list how many there are, such as recipes or randomly generated media, but more complex things like photos have no numbers.
 * The delete functions are quite simple; they give a "final chance" screen (and selection menu for memory cards) and add some cheeky flavor to the deletion.
 * Found media (morale photos and music) exists as nothing more than integers, so there is no way to prevent duplication other than preventing any copies.
 * My solution was to make it so any transfer of the photos and songs will remove the originals from whatever device they were previously on.
 * Its not perfect but it prevents bugs, and the MP3 player function has made these data types rather unappealing.
 * 
 * emovedata relies on a few extra functions, iuse::update_monsters, and clear_ebooks.
 * update_monsters is a set of about 30 lines of code to compare two strings of monster collection data, and return an updated string of data
 * the code itself was already in use by the original camera function, and calling upon the process multiple times for moving data became necessary.
 * clear_ebooks is a simple addition similar to the function ebooks that reaches thtrough item.h/cpp and item_contents.h/cpp in order to clear data.
 * This is because electronic books function via a pocket in the device that holds a weightless variant of the original book, while retaining access
 * to all the functions having that book physically with the player would provide, such as any recipes it contains.
 */
/*! @page estorage
 *  @brief Accesses stored data on the electronic device
 *  @par EStorage is meant to be an improvement to the flow of actions needed to access the various data items found in the game, which
 * in its previous state was a motley collection of different sets that appear to have been made by different people many years apart.
 * They did not play well together, and the tangled web of references and code that encompasses electronic tools seem to be rarely touched.
 *
 * The bulk of estorage is nothing more than putting the ability to view all of these data types into a single, modular iuse with some tweaks.
 * When the iuse is activated, it looks at the flags of the item it is called by to see what types of data it should be looking for, and constructs
 * a uimenu customized to those flags. With these flags, any item can be given modular electronic memory. 
 * It may not have a big impact on the game, but it allows for future additions
 * to be easier to implement and makes the code easier to read by putting viewing data and moving data into two condensed places, rather than scattered around.
 * It also makes the player's flow of accessing the stored data slightly faster and less clunky.
 *
 * A list and brief explanation of data types (under their new names)
 * Uploaded_Photos: Photos taken by the player using any item with the "Camera" iuse function.
 * Photos: Part of the morale system, these random photos appear in unread memory cards, and upon viewing have a chance 
 *    to provide a one-time boost to morale
 * Music: Random songs that appear in unread memory cards. Largely irrelevant with the existence of the MP3 player iuse function,
     which has unlimited music.
 * Recipes: Random crafting recipes that appear in unread memory cards. Based on my testing they act the same as a written book recipe 
 *    while the device carrying them is near the player, the same as a book containing crafting recipes.
 * Monsters: Photos taken by the player using any item with the "Camera" iuse function, that also contain a monster.
 *    The Camera iuse function detects these photos and saves them as a separate type.
 *    They carry a quality type and are treated more like an upgradable data entry, with higher quality photos replacing worse quality ones.
 * Legacy Data: most of the old data types from the spaghetti collection of storage functions that predated estorage.
 *    These are defined by their old names as "eink_monsters" and the prefix trio of "C_," "MC_," and "EIPC_".
 *    The only name that seemed consistent enough to hold to was "extended_photo," which is used in far more places
 *    If the estorage iuse is activated and finds any of these legacy types, it offers a recovery option to convert things into the new format.
 * This function is based off of the original einktablet iuse, with alterations to tweak it and expand its scope.
 *
 * In order for this function and/or related functions to work, it relies on the following:
 *    A declaration in the iuse.h file
 *    Several functions that must be declared prior in the iuse.cpp file:
 *        "static int get_quality_from_string"
 *  	  "static std::string photo_quality_name"
 *        "void item::extended_photo_def::deserialize"
 *        "void item::extended_photo_def::serialize"
 *		  "void 
 *    A declaration in itemfactory.cpp in order to read json files and assign the functions where appropriate
 *    The use of an activity actor for extended tasks, declared across multiple functions in activity_actor.cpp,
 *        with their behavior as a whole declared and described within activity_actor_definitions.h
 *    Any flags being used are declared within flag.h, flag.cpp, and within json/flags.json
 *        
 *  @author HackJob
 */
 /*! @page data_dnload_activity_actor
 *  @brief Scans a given vector of items for data vars, emplaces the data onto onother item, and prints out a tally of info
 *  @par The fuction is mostly just a heavily edited copy of the original data_handling_activity_actor.
 * There are a few sections to catch legacy data types, and it has been expanded to also handle downloading recipes and ebooks.
 * The new flag system is integrated into the activity as well. If a card has incompatible data for a device, the data simply isn't downloaded.
 * In particular, if an unread memory card is read by a device with "limited" ability, the random data is generated but left on the card.
 * @author HackJob
 */
 /*! @page iuse::camera
 *  @brief Talking about the Camera and related functions
 *  @par The Camera iuse and its related functions, which arguably encompasses anything regarding photographs (not including morale photos)
 * is probably the most convoluted and complex section of the item iuse.
 * It relies on a number of separately defined functions declared within iuse.cpp, and goes through an admirable amount of work to
 * create its photos, acounting for camera focus issues, the player being blinded, photobombing by monsters, excessive amounts of items
 * laying around, even the character hallucinating something and expecting to see it in the photograph!
 * Compounding this complexity is the monster photos, which contain a quality and require every addition of a new photo to memory undergo
 * a process where it compares the photo against existing entries, and keeping the superior version.
 * This appears to necessitate far more code than the extended photos, which can be updated properly in potentially as little as 4 lines
 */
 /*! @page iuse::remove_duplicate_photos
 *  @brief Prevents duplicate photos from being created.
 *  @par This had to be created to prevent infinite photocopying from the download/upload functions.
 * I believe the issue started because the original camera function was very concrete in its flow and would remove photos on copying.
 * It works by making 3 empty vectors of photos. the first gets to hold a copy of the photos to be checked.
 * One by one it goes through the starting collection, comparing the strings of the photo contents.
 * If it doesn't find a matching string in the initially empty 2nd vector, it puts a copy there, and another copy in the 3rd vector.
 * Once it has run through every photo in the first vector, it overwrites the source variable of photos with its checked list.
 * @author HackJob
 */
 