#pragma once

enum TextureType : unsigned char {
    TERRAIN, INVENTORY, NUMBER
};

enum BlockType : unsigned char
{
  EMPTY,
  DNE, //1
  /* etc types */
  GRASS,
  DIRT,
  STONE,
  SAND,
  SNOW,
  BEDROCK, //7
  /* terrain blocks */
  LAVA,
  WATER,
  ICE, //10
  /* fluids */
  WOOD,
  LEAF,
  CACTUS, //13
  /* terrain features */
  DIAMOND_ORE,
  COAL,
  REDSTONE_ORE,
  IRON_ORE,
  GOLD_ORE //18
  /* ores */
};