#pragma once
#include "esp_random.h"

static const char *dinoNames[50] = {
    "TinyRx", "Jumpa", "Cacto", "Fossy", "Scale", "NomNom", "Rawry", "Dippy",
    "Trixy", "Stompy", "Pebble", "Snappy", "Raptor", "Clawzy", "Spikey", "Chompy",
    "Zoomy", "Rocky", "Duney", "Fossil", "Rexie", "Crunch", "Drako", "Lizard",
    "Roary", "Ptero", "Dusty", "Snout", "Taily", "Craty", "Hoppy", "Zilla",
    "Scurry", "Boulder", "Ashrx", "Cliffy", "Saury", "Terra", "Nomzer", "Fangy",
    "Bronto", "Cacty", "Duner", "Jumpo", "Rumbly", "Roxy", "Fossix", "Spike",
    "Chomps", "Reezy"
};



static const char *get_random_dino_name()
{
    int index = esp_random() % 50;
    return dinoNames[index];
}

static const char *getDinoName()
{
    static const char *name = nullptr;
    if (!name)
        name = get_random_dino_name();
    return name;
}
