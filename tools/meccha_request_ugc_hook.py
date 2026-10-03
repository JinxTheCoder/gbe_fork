from pathlib import Path

path = Path("dll/steam_ugc.cpp")
text = path.read_text(encoding="utf-8")

old = '''SteamAPICall_t Steam_UGC::RequestUGCDetails_old( PublishedFileId_t nPublishedFileID, uint32 unMaxAgeSeconds )
{
    PRINT_DEBUG("%llu", nPublishedFileID);

    // MECCHA v11: RequestUGCDetails is metadata only.
    // Never open download consent from this API.
    return internal_RequestUGCDetails(
        nPublishedFileID,
        unMaxAgeSeconds,
        IUgcItfVersion::v018
    );
}
'''

new = '''SteamAPICall_t Steam_UGC::RequestUGCDetails_old( PublishedFileId_t nPublishedFileID, uint32 unMaxAgeSeconds )
{
    PRINT_DEBUG("%llu", nPublishedFileID);

#ifdef _WIN32
    // MECCHA v16: the game's Add Workshop Content button reaches this old
    // details API before it attempts to hand the missing item to Steam.
    // Route that missing-item request into the same local ZIP import dialog
    // used by F8, then refresh the live cache before returning details.
    refresh_live_workshop_cache(settings, ugc_bridge);

    if (!settings->isModInstalled(nPublishedFileID)) {
        const std::string requested_id =
            std::to_string(static_cast<unsigned long long>(nPublishedFileID));
        std::string imported_id;

        PRINT_DEBUG(
            "MECCHA RequestUGCDetails_old missing item %llu; opening F8 import dialog",
            nPublishedFileID
        );

        MecchaWorkshopUI::show_f8_import_dialog(
            requested_id,
            std::filesystem::u8path(Local_Storage::get_game_settings_path()),
            imported_id
        );

        refresh_live_workshop_cache(settings, ugc_bridge);

        if (!settings->isModInstalled(nPublishedFileID)) {
            PRINT_DEBUG(
                "MECCHA RequestUGCDetails_old item %llu still missing; blocking remote Steam fallback",
                nPublishedFileID
            );
            return k_uAPICallInvalid;
        }

        PRINT_DEBUG(
            "MECCHA RequestUGCDetails_old item %llu imported; returning local details",
            nPublishedFileID
        );
    }
#endif

    return internal_RequestUGCDetails(
        nPublishedFileID,
        unMaxAgeSeconds,
        IUgcItfVersion::v018
    );
}
'''

if old in text:
    path.write_text(text.replace(old, new, 1), encoding="utf-8", newline="\n")
    print("Applied MECCHA RequestUGCDetails_old F8 import hook")
elif "MECCHA v16: the game's Add Workshop Content button reaches this old" in text:
    print("MECCHA RequestUGCDetails_old F8 import hook is already applied")
else:
    raise SystemExit("Expected RequestUGCDetails_old block was not found; source changed")
