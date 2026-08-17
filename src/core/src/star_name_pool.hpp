#pragma once

#include <cstddef>
#include <string_view>

namespace suns {

// The pool deliberately mixes public-domain / factual naming traditions with
// original names. Categories are comments for maintainers only; the game
// presents one shuffled name space so generated galaxies feel eclectic rather
// than procedurally syllabic.
inline constexpr std::string_view kCuratedStarNames[] = {
    // Astronomy — real star names.
    "Altair", "Deneb", "Vega", "Sirius", "Procyon", "Capella", "Arcturus", "Aldebaran", "Regulus", "Spica",
    "Antares", "Fomalhaut", "Bellatrix", "Alnilam", "Alnitak", "Saiph", "Mirach", "Almach", "Mizar", "Alcor",
    "Dubhe", "Merak", "Phecda", "Megrez", "Alioth", "Alkaid", "Polaris", "Kochab", "Sadr", "Gienah",
    "Rasalhague", "Caph", "Schedar", "Diphda", "Menkar", "Markab", "Scheat", "Alpheratz", "Hamal", "Algol",
    "Mirfak", "Eltanin", "Thuban", "Unukalhai", "Zosma", "Vindemiatrix", "Alphard", "Adhara", "Wezen", "Nashira",

    // Mythology and ancient tradition.
    "Anansi", "Freya", "Enki", "Ishtar", "Selene", "Nyx", "Eos", "Themis", "Rhea", "Dione",
    "Tethys", "Hyperion", "Asteria", "Brigid", "Lugh", "Mimir", "Skadi", "Tyr", "Heimdall", "Thoth",
    "Ptah", "Bastet", "Sekhmet", "Horus", "Amun", "Inanna", "Nergal", "Tiamat", "Gilgamesh", "Marduk",
    "Amaterasu", "Susanoo", "Izanami", "Izanagi", "Hestia", "Ariadne", "Calliope", "Mnemosyne", "Morrigan", "Cernunnos",

    // Geography and natural regions.
    "Tunguska", "Lena", "Yukon", "Orinoco", "Atacama", "Pamir", "Altai", "Ural", "Danube", "Volga",
    "Baikal", "Sahara", "Kalahari", "Patagonia", "Kamchatka", "Tasmania", "Bering", "Luzon", "Sumatra", "Sunda",
    "Tigris", "Euphrates", "Indus", "Mekong", "Niger", "Congo", "Zambezi", "Okavango", "Sinai", "Gobi",
    "Andes", "Caucasus", "Carpathia", "Anatolia", "Siberia", "Lapland", "Kodiak", "Aleut", "Socotra", "Zanzibar",

    // Science and mathematics.
    "Faraday", "Maxwell", "Ampere", "Volta", "Gauss", "Kepler", "Galileo", "Hubble", "Herschel", "Curie",
    "Fermi", "Dirac", "Noether", "Euler", "Fourier", "Laplace", "Lagrange", "Poisson", "Cavendish", "Joule",
    "Kelvin", "Planck", "Boltzmann", "Raman", "Saha", "Chandrasekhar", "Tesla", "Newton", "Hooke", "Huygens",
    "Brahe", "Halley", "Messier", "Fraunhofer", "Bessel", "Airy", "Lorentz", "Rutherford", "Bohr", "Heisenberg",

    // Nature, minerals and atmospheric words.
    "Juniper", "Kestrel", "Raven", "Falcon", "Heron", "Ibis", "Lynx", "Sable", "Osprey", "Viper",
    "Mantis", "Cicada", "Coral", "Amber", "Jade", "Onyx", "Quartz", "Basalt", "Granite", "Opal",
    "Jasper", "Agate", "Beryl", "Topaz", "Zircon", "Garnet", "Obsidian", "Flint", "Cobalt", "Indigo",
    "Saffron", "Umber", "Cinder", "Frost", "Tundra", "Taiga", "Monsoon", "Zephyr", "Mistral", "Sirocco",
    "Borealis", "Tempest", "Solstice", "Equinox", "Meridian", "Horizon", "Nocturne", "Eclipse", "Ember", "Nimbus",

    // Historic places and old cultural names.
    "Delphi", "Samarkand", "Persepolis", "Carthage", "Ithaca", "Byblos", "Petra", "Palmyra", "Knossos", "Tyre",
    "Memphis", "Thebes", "Uruk", "Nineveh", "Susa", "Bactria", "Rhodes", "Cyrene", "Ephesus", "Miletus",
    "Taxila", "Aksum", "Meroe", "Troy", "Corinth", "Sparta", "Nara", "Kyoto", "Tikal", "Cusco",

    // Original Suns! names — curated rather than assembled at runtime.
    "Averin", "Belisar", "Cyrane", "Doven", "Eredane", "Falorin", "Helion", "Iskara", "Kestara", "Lareth",
    "Mavren", "Neris", "Orren", "Phael", "Quorin", "Rhyssa", "Sereph", "Talven", "Uldra", "Veyra",
    "Westrin", "Ysil", "Zeran", "Avarra", "Brannis", "Cendrel", "Doreth", "Elarin", "Feyra", "Gavren",
    "Hadris", "Ilyra", "Kaelon", "Lorven", "Meryn", "Nadira", "Othren", "Perrin", "Quessa", "Raveth",
    "Serin", "Tavros", "Uvena", "Vardis", "Weyra", "Xeran", "Yvara", "Zorin", "Aureth", "Nemeris",
};

inline constexpr std::size_t kCuratedStarNameCount = sizeof(kCuratedStarNames) / sizeof(kCuratedStarNames[0]);
static_assert(kCuratedStarNameCount >= 64, "The curated pool must cover the largest supported galaxy without reuse");

} // namespace suns
