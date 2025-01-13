#!/bin/bash
ASSET_GENERATOR=./generate_assets.py
INCLUDE_FOLDER=src/include/assets
mkdir -p ${INCLUDE_FOLDER}/models
mkdir -p ${INCLUDE_FOLDER}/icons
mkdir -p ${INCLUDE_FOLDER}/templates
${ASSET_GENERATOR} assets/no_cover_art.gif NoCoverArtGif  > ${INCLUDE_FOLDER}/no_cover_art.h
${ASSET_GENERATOR} assets/favicon.ico Favicon             > ${INCLUDE_FOLDER}/favicon.h
${ASSET_GENERATOR} assets/style.qss Stylesheet  > ${INCLUDE_FOLDER}/no_cover_art.h
${ASSET_GENERATOR} assets/no_cover_art.gif NoCoverArtGif  > ${INCLUDE_FOLDER}/no_cover_art.h
${ASSET_GENERATOR} assets/no_cover_art.gif NoCoverArtGif  > ${INCLUDE_FOLDER}/no_cover_art.h

${ASSET_GENERATOR} models/genre_discogs400-discogs-effnet-1.pb DiscogsGenreTFGraph > ${INCLUDE_FOLDER}/models/genre_discogs400-discogs-effnet.h
${ASSET_GENERATOR} models/discogs-effnet-bs64-1.pb DiscogsEffnetBS64 > ${INCLUDE_FOLDER}/models/discogs_effnet_bs64.h

${ASSET_GENERATOR} assets/templates/track_analysis.html TrackAnalysisTemplate > ${INCLUDE_FOLDER}/templates/track_analysis.h
${ASSET_GENERATOR} assets/templates/player_info.html PlayerInfoTemplate > ${INCLUDE_FOLDER}/templates/player_info.h
${ASSET_GENERATOR} assets/icons/stop.svg StopIcon > ${INCLUDE_FOLDER}/icons/stop.h
${ASSET_GENERATOR} assets/icons/skip.svg SkipIcon > ${INCLUDE_FOLDER}/icons/skip.h 
${ASSET_GENERATOR} assets/icons/play.svg PlayIcon > ${INCLUDE_FOLDER}/icons/play.h
${ASSET_GENERATOR} assets/icons/pause.svg PauseIcon > ${INCLUDE_FOLDER}/icons/pause.h