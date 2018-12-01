#include "stdafx.h"
#include "MiniTerrian.h"


MiniTerrain::MiniTerrain()
{}

void MiniTerrain::CreateColumn(glm::vec2 colPos) {
        glm::ivec2 world(colPos);
        glm::vec2 grid = MiniBiomeGenerator::blockToGrid(world.x, world.y);
        float p1, p2, p3 = 0.0f;
        BlockType b2 = EMPTY;
        BlockType b3 = EMPTY;
        BlockType b1 = MiniBiomeGenerator::determineBiome(grid, colPos, p1, p2, p3, b2, b3);
        float colHeightPercent = MiniFBM::fbm(colPos / continuity);

        //use fill column when it is improved. For now do not interpolate biome heights
        //fillColumn(b1, b2, b3,colPos, colHeightPercent, p1, p2, p3);
        int h1;
        switch (b1) {
        case SAND:
            h1 = 128 + colHeightPercent * 80;
            for (int y = 0; y < 256; ++y) {
                if (y < 127) {
                    setBlockAt(colPos.x, y, colPos.y, STONE);
                } else if (y < h1) {
                    setBlockAt(colPos.x, y, colPos.y, SAND);
                } else if (y == h1) {
                    setBlockAt(colPos.x, y, colPos.y, SAND);
                } else {
                    setBlockAt(colPos.x, y, colPos.y, EMPTY);
                }
            }
            break;
        case GRASS:
            h1 = 128 + colHeightPercent * 80;
            for (int y = 0; y < 256; ++y) {
                if (y < 127) {
                    setBlockAt(colPos.x, y, colPos.y, STONE);
                } else if (y < h1) {
                    setBlockAt(colPos.x, y, colPos.y, DIRT);
                } else if (y == h1) {
                    setBlockAt(colPos.x, y, colPos.y, GRASS);
                } else {
                    setBlockAt(colPos.x, y, colPos.y, EMPTY);
                }
            }
            break;
        case SNOW:
            h1 = 128 + colHeightPercent * 80;
            for (int y = 0; y < 256; ++y) {
                if (y < 127) {
                    setBlockAt(colPos.x, y, colPos.y, STONE);
                } else if (y < h1) {
                    setBlockAt(colPos.x, y, colPos.y, SNOW);
                } else if (y == h1) {
                    setBlockAt(colPos.x, y, colPos.y, SNOW);
                } else {
                    setBlockAt(colPos.x, y, colPos.y, EMPTY);
                }
            }
            break;
        case WATER:
            h1 = 128 + colHeightPercent * 50;
            for (int y = 0; y < 256; ++y) {
                if (y < 100) {
                    setBlockAt(colPos.x, y, colPos.y, STONE);
                } else if (y < 110) {
                    setBlockAt(colPos.x, y, colPos.y, DIRT);
                } else if (y < h1) {
                    setBlockAt(colPos.x, y, colPos.y, WATER);
                } else if (y == h1) {
                    setBlockAt(colPos.x, y, colPos.y, WATER);
                } else {
                    setBlockAt(colPos.x, y, colPos.y, EMPTY);
                }
            }
            break;
        default:
            break;
        }
//    float colHeightPercent = FBM::fbm(colPos / continuity);
//    int colHeight = 128 + colHeightPercent * 50;
//    for (int y = 0; y < 256; ++y) {
//        if (y < 127) {
//            setBlockAt(colPos.x, y, colPos.y, STONE);
//        } else if (y < colHeight) {
//            setBlockAt(colPos.x, y, colPos.y, DIRT);
//        } else if (y == colHeight) {
//            setBlockAt(colPos.x, y, colPos.y, GRASS);
//        } else {
//            setBlockAt(colPos.x, y, colPos.y, EMPTY);
//        }
//    }
}

uint64_t MiniTerrain::ConvertChunkToKey(glm::ivec3 chunkCoord) {
    uint64_t x64Bit = (uint64_t) ((uint32_t) chunkCoord.x);
    uint64_t z64Bit = (uint64_t) ((uint32_t) chunkCoord.z);
    return (x64Bit << 32) | z64Bit;
}

glm::ivec3 MiniTerrain::ConvertKeyToChunk(uint64_t key) {
    return glm::ivec3((int) (key >> 32), 0, (int) key);
}

glm::ivec3 MiniTerrain::ConvertWorldToChunk(glm::ivec3 world) {
    int chunkX = (world.x / X_WIDTH) - (((world.x < 0) && (world.x % X_WIDTH)) ? 1 : 0);
    int chunkZ = (world.z / Z_WIDTH) - (((world.z < 0) && (world.z % Z_WIDTH)) ? 1 : 0);
    return glm::ivec3(chunkX, world.y, chunkZ);
}

glm::ivec3 MiniTerrain::ConvertChunkToWorld(glm::ivec3 chunk) {
    return glm::ivec3(chunk.x * X_WIDTH, chunk.y, chunk.z * Z_WIDTH);
}

glm::ivec3 MiniTerrain::ConvertWorldToChunkLocal(glm::ivec3 world) {

    /* get the bottom left corner of chunk in world space coords */
    glm::ivec3 chunk = ConvertWorldToChunk(world);
    glm::ivec3 chunkBLC = ConvertChunkToWorld(chunk);

    return glm::ivec3(world.x - chunkBLC.x, world.y, world.z - chunkBLC.z);
}

glm::ivec3 MiniTerrain::ConvertChunkLocalToWorld(glm::ivec3 chunkLocal, const Chunk *chunk) {
    return chunkLocal + ConvertChunkToWorld(ConvertKeyToChunk(chunk->key));
}


BlockType MiniTerrain::getBlockAt(int x, int y, int z) const
{
    // validate input
    if (y >= Y_WIDTH || y < 0) {/*std::cout << "Invalid Y for getBlockAt" << std::endl;*/ return DNE; }

    // then figure out which chunk this point is in
    uint64_t key = ConvertChunkToKey(ConvertWorldToChunk(glm::ivec3(x,y,z)));
    auto found_key = chunkMap.find(key);
    if (found_key != std::end(chunkMap)) {
        // then get the reference value from the Chunk and edit it
        glm::ivec3 local = ConvertWorldToChunkLocal(glm::ivec3(x,y,z));
        return found_key->second->getBlockAtLocal(local.x, local.y, local.z);
    } else {
        return DNE;
    }
}

int MiniTerrain::getHeightOfTerrainAtColumn(int x, int z) const
{
    BlockType curr = DNE;
    for (int y = 0; y < Y_WIDTH; y++) {
        curr = getBlockAt(x, y, z);
        if (curr == EMPTY && y != 0) {
            return y - 1;
        }
    }
    return 0;
}

void MiniTerrain::generateRiver(RiverType type, glm::vec3 position, glm::vec3 orientation) {
    /* Create Lsystem and carve out every line */
    MiniLSystem riverGenerator(type, position, orientation);
    std::pair<glm::vec3, glm::vec3> emptyPair;
    std::pair<glm::vec3, glm::vec3> line = riverGenerator.getNextLine();
    float yValue = this->getHeightOfTerrainAtColumn(line.first.x, line.first.z);
    while (line != emptyPair) {
        // TODO: Refactor so that setBlockAt designates chunks as to draw
        this->designateChunkAsToDraw(line.first.x, line.first.y, line.first.z);
        this->designateChunkAsToDraw(line.first.x + CARVING_WIDTH, line.first.y, line.first.z);
        this->designateChunkAsToDraw(line.first.x, line.first.y, line.first.z + CARVING_WIDTH);
        this->designateChunkAsToDraw(line.first.x + CARVING_WIDTH, line.first.y, line.first.z + CARVING_WIDTH);
        this->carveRiverAtLine(line, yValue);
        line = riverGenerator.getNextLine();
    }
}

void MiniTerrain::generateCave(glm::vec3 position, glm::vec3 orientation) {
    /* Create Lsystem and carve out every line */
    MiniLSystem caveGenerator(CAVE, position, orientation);
    std::pair<glm::vec3, glm::vec3> emptyPair;
    std::pair<glm::vec3, glm::vec3> line = caveGenerator.getNextLine();
    while (line != emptyPair) {
        this->carveCaveAtLine(line);
        line = caveGenerator.getNextLine();
    }
}

void MiniTerrain::generateBiomeSpecificTerrainFeature(glm::ivec3 position) {
    /* get the biome that we're in */
    glm::vec2 grid = MiniBiomeGenerator::blockToGrid(position.x, position.z);
    BlockType dummyBlock1 = EMPTY;
    BlockType dummyBlock2 = EMPTY;
    float dummyFloat = 0.0f;
    BlockType biomeType = MiniBiomeGenerator::determineBiome(grid, glm::vec2(position.x, position.z), dummyFloat, dummyFloat, dummyFloat, dummyBlock1, dummyBlock2);

    /* determine what to generate */
    switch(biomeType) {
        case GRASS :
            generateTree(position);
            break;
        case SNOW :
            generateIcicle(position);
            break;
        case WATER :
            generateIsland(position);
            break;
        case SAND :
            generateCactus(position);
            break;
        default :
            break;
    }
}

void MiniTerrain::generateTree(glm::ivec3 position) {
    /* first make sure we aren't on the boundary of a chunk, if so return */
    glm::ivec3 chunkLocalCoords = MiniTerrain::ConvertWorldToChunkLocal(position);
    if (chunkLocalCoords.x % X_WIDTH - 1 < 4 ||
        chunkLocalCoords.x % X_WIDTH - 1 > X_WIDTH - 4 ||
        chunkLocalCoords.z % Z_WIDTH - 1 < 4 ||
        chunkLocalCoords.z % Z_WIDTH - 1 > Z_WIDTH - 4) {
        return;
    }

    int terrainY = getHeightOfTerrainAtColumn(position.x, position.z);
    /* random tree height between 5 and 8 */
    float treeHeight = ((float) rand()/ (float) (RAND_MAX / 3)) + 5;
    float i = 0;

    /* generate the tree trunk */
    for (; i < treeHeight; i++) {
        setBlockAt(position.x, terrainY + 1 + i, position.z, WOOD);
    }

    /* random leaf height between 3 and 4*/
    int leafWidth = ((float) rand()/ (float) (RAND_MAX / 2)) + 3;

    /* generate the leaves in a cone-like shape*/
    for (int y = 0; y <= leafWidth; y++) {
        for (int x = -(leafWidth - y); x <= (leafWidth - y); x++) {
            for (int z = -(leafWidth - y); z <= (leafWidth - y); z++) {
                BlockType blockToMakeLeaf = getBlockAt(position.x + x, terrainY + (1 + i - 3) + y, position.z + z);
                if (blockToMakeLeaf == EMPTY || blockToMakeLeaf == WOOD) {
                    setBlockAt(position.x + x, terrainY + (1 + i - 3) + y, position.z + z, LEAF);
                }
            }
        }
    }
}

void MiniTerrain::generateIcicle(glm::ivec3 position) {
    position.y = getHeightOfTerrainAtColumn(position.x, position.z);

    /* first make sure we aren't on the boundary of a chunk, if so return */
    glm::ivec3 chunkLocalCoords = MiniTerrain::ConvertWorldToChunkLocal(position);
    if (chunkLocalCoords.x % X_WIDTH - 1 < 4 ||
        chunkLocalCoords.x % X_WIDTH - 1 > X_WIDTH - 4 ||
        chunkLocalCoords.z % Z_WIDTH - 1 < 4 ||
        chunkLocalCoords.z % Z_WIDTH - 1 > Z_WIDTH - 4) {
        return;
    }

    /* random icicle radius between 2 and 4 */
    float icicleRadius = ((float) rand()/ (float) (RAND_MAX / 3)) + 2;

    /* create the icicle */
    for (int x = 0; x < icicleRadius; ++x) {
        for (int y = 0; y < icicleRadius; ++y) {
            for (int z = 0; z < icicleRadius; ++z) {
                glm::ivec3 posToMakeIce = position + glm::ivec3(x,y,z);
                if (glm::distance(glm::vec3(posToMakeIce), glm::vec3(position)) < icicleRadius) {
                    setBlockAt(posToMakeIce.x, posToMakeIce.y, posToMakeIce.z, ICE);
                }
            }
        }
    }
}

void MiniTerrain::generateIsland(glm::ivec3 position) {
    /* first make sure we aren't on the boundary of a chunk, if so return */
    glm::ivec3 chunkLocalCoords = MiniTerrain::ConvertWorldToChunkLocal(position);
    if (chunkLocalCoords.x % X_WIDTH - 1 < 6 ||
        chunkLocalCoords.x % X_WIDTH - 1 > X_WIDTH - 6 ||
        chunkLocalCoords.z % Z_WIDTH - 1 < 6 ||
        chunkLocalCoords.z % Z_WIDTH - 1 > Z_WIDTH - 6) {
        return;
    }

    position.y = getHeightOfTerrainAtColumn(position.x, position.z);

    /* random island radius between 4 and 7 */
    float islandRadius = ((float) rand()/ (float) (RAND_MAX / 4)) + 4;

    /* create the island */
    for (int x = -islandRadius; x <= islandRadius; ++x) {
        for (int y = 0; y <= islandRadius; ++y) {
            for (int z = -islandRadius; z <= islandRadius; ++z) {
                /* carve out the hemisphere */
                glm::ivec3 posToMakeLand = position + glm::ivec3(x,y,z);
                if (glm::distance(glm::vec3(posToMakeLand), glm::vec3(position)) < islandRadius) {
                    setBlockAt(posToMakeLand.x, posToMakeLand.y, posToMakeLand.z, DIRT);
                    /* go down until you reach non water and fill-in stone */
                    if (y == 0) {
                        posToMakeLand.y--;
                        while (getBlockAt(posToMakeLand.x, posToMakeLand.y, posToMakeLand.z) == WATER) {
                            setBlockAt(posToMakeLand.x, posToMakeLand.y, posToMakeLand.z, STONE);
                            posToMakeLand.y--;
                        }
                    }
                } else if (glm::distance(glm::vec3(posToMakeLand), glm::vec3(position)) == islandRadius) {
                    setBlockAt(posToMakeLand.x, posToMakeLand.y, posToMakeLand.z, GRASS);
                }
            }
        }
    }
}

void MiniTerrain::generateCactus(glm::ivec3 position) {
    int terrainY = getHeightOfTerrainAtColumn(position.x, position.z);
    /* random cactus height between 3 and 6 */
    int cactusHeight = ((float) rand()/ (float) (RAND_MAX / 3)) + 3;
    /* generate the cactus */
    for (int i = 0; i < cactusHeight; i++) {
        setBlockAt(position.x, terrainY + 1 + i, position.z, CACTUS);
    }
}

void MiniTerrain::designateChunkAsToDraw(int x, int y, int z) {
    uint64_t key = ConvertChunkToKey(ConvertWorldToChunk(glm::ivec3(x,y,z)));
    if (this->chunkExists(key)) {
      auto found_chunk = std::find_if(std::begin(program_state->chunksToBeDrawn), std::end(program_state->chunksToBeDrawn), [&](const auto& chunk)
      {
        return chunk == this->chunkMap[key];
      });

      if (found_chunk == std::end(program_state->chunksToBeDrawn)) {
          program_state->chunksToBeDrawn.push_back(this->chunkMap[key]);
      }
    }
}

void MiniTerrain::carveRiverAtLine(std::pair<glm::vec3, glm::vec3> line, int yValue)
{
    /* get the world space x and z coordinates of the two points in the river */
    glm::vec3 start(line.first.x, line.first.y, line.first.z);
    glm::vec3 end(line.second.x, line.second.y, line.second.z);

    /* if one of the two points is in a chunk that doesn't exist yet, return */
    if (start.y == 0 || end.y == 0 || end == start) {
        return;
    } else {
        start.y = 0;
        end.y = 0;
    }

    /* get the direction and length of this branch of the river */
    float length = glm::length(end - start);
    glm::vec3 riverDirection = glm::normalize(end - start);
    glm::vec3 riverRightVector = glm::normalize(glm::cross(riverDirection, glm::vec3(0, 1, 0)));

    /* march along the direction and carve the river at each point */
    for (float step = 0; step < length; step += 0.5) {
        glm::vec3 march = start + (step * riverDirection);
        carveAroundPoint(glm::vec3(march.x, yValue - CARVING_DEPTH, march.z), riverRightVector, riverDirection, 3, 3);
    }
}

void MiniTerrain::carveCaveAtLine(std::pair<glm::vec3, glm::vec3> line) {
    /* get the world space x and z coordinates of the two points in the cave */
    glm::vec3 start(line.first.x, line.first.y, line.first.z);
    glm::vec3 end(line.second.x, line.second.y, line.second.z);

    /* sanity check, line should be a line (prevent normalize NaN) */
    if (end == start) return;

    /* get the direction and length of this branch of the cave */
    float length = glm::length(end - start);
    glm::vec3 caveDirection = glm::normalize(end - start);

    /* determine if we're spawning a cavern at this line */
    float cavernSpawn = ((float) rand()/ (float) (RAND_MAX));

    /* If we're a lava cavern, then make the cavern halfway through  */
    if (cavernSpawn < LAVA_CAVERN_CHANCE && line.second.y < 60) {
        for (float step = 0; step < length; step += 0.5) {
            glm::vec3 march = start + (step * caveDirection);

            /* if we're in first or last quarter, carve normally, else make the cavern */
            if (step < length/4.0f || step > (length*3.0f)/4.0f) {
                carveSphereAroundPoint(march, CAVE_WIDTH, false);
            } else if ((step < length/2.0f + 0.25) && step > (length/2.0f - 0.25)) {
                this->carveSphereAroundPoint(march - glm::vec3(0,4,0), LARGE_CAVERN_SIZE, true);
            }
        }
    } else {
        /* march along the direction and carve a sphere at each point */
        for (float step = 0; step < length; step += 0.5) {
            glm::vec3 march = start + (step * caveDirection);
            carveSphereAroundPoint(march, CAVE_WIDTH, false);
        }
    }
}

void MiniTerrain::carveAroundPoint(glm::vec3 point, glm::vec3 riverRightVector, glm::vec3 riverDirection, int width, int depth) {
    /* carve out to the left and right of the point */
    riverRightVector.y = 0;

    for (int k = -1; k < 2; k++) {
        for (int q = -1; q < 2; q++) {
            carveSlopeInDirection(point.x + k, point.y, point.z + q, riverRightVector);
            carveSlopeInDirection(point.x + k, point.y, point.z + q, (riverRightVector * -1.0f));
            // TODO: Linearly interpolate from right to left and carve out in front of the river as well
            //carveSlopeInDirection(point.x + k, point.y, point.z + q, riverDirection);
        }
    }

    /* fill in the other blocks with water */
    for (float i = 0; i < width; i++) {
        glm::vec3 right = point + (riverRightVector * i);
        glm::vec3 left = point - (riverRightVector * i);
        clearTerrainAboveBlock(right.x, right.y, right.z);
        clearTerrainAboveBlock(left.x, left.y, left.z);
        for (int j = 0; j < depth; j++) {
            setBlockAt(right.x, right.y - j, right.z, WATER);
            setBlockAt(left.x, left.y - j, left.z, WATER);
        }
    }
}

void MiniTerrain::carveSphereAroundPoint(glm::vec3 center, int radius, bool isLavaCavern) {

    /* multiply the radius of the sphere we are hollowing out by a random value to make caves cavey */
    if (!isLavaCavern) {
        float caveSize = ((float) rand()/ (float) (RAND_MAX)) * CAVE_SPHERE_WIDTH;
        radius *= caveSize;
    }

    /* iterate through all points in the Width*width*width cube and carve if they are within radius of center */
    for (int x = isLavaCavern ? -radius : 0; x < radius; ++x) {
        for (int y = 0; y < radius; ++y) {
            for (int z = isLavaCavern ? -radius : 0; z < radius; ++z) {
                glm::vec3 posToCarve = center + glm::vec3(x,y,z);
                if (glm::distance(posToCarve, center) < radius) {
                    /* if we are constructing a lava cavern, make the floor lava */
                    if (isLavaCavern && (y <= 1)) {
                        setBlockAt(posToCarve.x, posToCarve.y, posToCarve.z, LAVA);
                    } else {
                        setBlockAt(posToCarve.x, posToCarve.y, posToCarve.z, EMPTY);
                    }
                } else if (glm::distance(posToCarve, center) == radius)  {
                    maybeGenerateOreVeinAtPoint(posToCarve);
                }
            }
        }
    }
}

void MiniTerrain::maybeGenerateOreVeinAtPoint(glm::vec3 point) {
    BlockType type = getBlockAt(point.x, point.y, point.z);
    BlockType newType = type;
    if (type == STONE) {
        float noise = (float) rand()/ (float) RAND_MAX;
        if (noise < DIAMOND_DEPOSIT_THRESHOLD) {
            newType = DIAMOND_ORE;
        } else if (noise < GOLD_DEPOSIT_THRESHOLD) {
            newType = GOLD_ORE;
        } else if (noise < REDSTONE_DEPOSIT_THRESHOLD) {
            newType = REDSTONE_ORE;
        } else if (noise < IRON_DEPOSIT_THRESHOLD) {
            newType = IRON_ORE;
        } else if (noise < COAL_DEPOSIT_THRESHOLD) {
            newType = COAL;
        } else {
            return;
        }

        /* create random number between 2 and 8 */
        int vein_size = ((float) rand()/ (float) (RAND_MAX/6)) + 2;
        for (int x = 0; x < ORE_VEIN_RADIUS; ++x) {
            for (int y = 0; y < ORE_VEIN_RADIUS; ++y) {
                for (int z = 0; z < ORE_VEIN_RADIUS; ++z) {
                    if (getBlockAt(point.x + x, point.y + y, point.z + z) == STONE && vein_size > 0) {
                        setBlockAt(point.x + x, point.y + y, point.z + z, newType);
                        vein_size -= 1;
                    }
                }
            }
        }
    }
}

void MiniTerrain::carveSlopeInDirection(int x, int y, int z, glm::vec3 direction) {
    /* sample the terrain height in the direction */
    glm::vec3 slopeHeightSamplePoint = glm::vec3(x,y,z) + (direction * CARVING_WIDTH);
    int endHeight = this->getHeightOfTerrainAtColumn(slopeHeightSamplePoint.x, slopeHeightSamplePoint.z);
    int heightDifference = endHeight - y;
    float stepProportion = 1.0f;/*(float) heightDifference / (float) CARVING_WIDTH*/;

    /* then linearly interpolate to get the height to drop the terrain by */
    glm::vec3 start(x,y,z);
    float carvingDistance = heightDifference <= 0 ? CARVING_WIDTH : heightDifference;
    for (float i = 0; i < carvingDistance; i += stepProportion) {
        glm::vec3 march = start + (direction * i);
        clearTerrainAboveBlock(march.x, (y + i), march.z);
        if (getBlockAt(march.x, (y + i), march.z) == DIRT) {
            setBlockAt(march.x, (y + i), march.z, GRASS);
        }
    }
}

void MiniTerrain::clearTerrainAboveBlock(int x, int y, int z) {
    for (int i = y + 1; i < Y_WIDTH; i++) { // TODO: refactor this because its slow af
        setBlockAt(x, i, z, EMPTY);
    }
}

void MiniTerrain::setBlockAt(int x, int y, int z, BlockType t)
{
    // validate input
    if (y >= Y_WIDTH || y < 0) { /*std::cout << "Invalid Y for setBlockAt: " << y << std::endl;*/ return; }

    // then figure out which chunk this point is in
    uint64_t key = ConvertChunkToKey(ConvertWorldToChunk(glm::ivec3(x,y,z)));

    if (chunkMap.find(key) != std::end(chunkMap)) {
        // then get the reference value from the Chunk and edit it
        glm::ivec3 local = ConvertWorldToChunkLocal(glm::ivec3(x,y,z));
        Chunk* chunk = chunkMap[key];
        BlockType& block = chunk->getBlockRefAtLocal(local.x, local.y, local.z);
        block = t;
        //this->designateChunkAsToDraw(x, y, z);
    } else {
        /*glm::ivec3 c = ConvertWorldToChunk(glm::ivec3(x,y,z));
        std::cerr << "Error: setBlockAt World:{"<<x<<", "<<y<<", "<<z<<"} ";
        std::cerr << "-- Chunk:{"<<c.x<<", "<<c.z<<"} DNE" << std::endl;*/
    }
}

Chunk::Chunk(uint64_t key, MiniTerrain* terrain)
    : key(key), blocks(), terrain(terrain)
{
    terrain->chunkMap.insert({key, this});
}

void Chunk::populateChunk() {
    // Get our chunk's world space x and z from its key
    glm::ivec3 world = MiniTerrain::ConvertChunkToWorld(MiniTerrain::ConvertKeyToChunk(this->key));

    // Loop through the world space coordinates of our chunk and create columns
    for (int x = 0; x < X_WIDTH; ++x) {
        for (int z = 0; z < Z_WIDTH; ++z) {
            terrain->CreateColumn(glm::vec2(world.x + x, world.z + z));
        }
    }

    // Now randomly decide if we're going to create a terrain feature
    float featureVal = ((float) rand()/ (float) (RAND_MAX));
    if (featureVal < FEATURE_CREATE_THRESHOLD) {
        // randomly decide on the local coordinate to generate the feature
        // (make sure it's not at the edge of a chunk)
        int localFeatureX = ((float) rand()/ (float) (RAND_MAX / X_WIDTH));
        int localFeatureZ = ((float) rand()/ (float) (RAND_MAX / Z_WIDTH));
        glm::ivec3 localFeatureCoords(localFeatureX, 0, localFeatureZ);
        glm::ivec3 worldFeatureCoords = MiniTerrain::ConvertChunkLocalToWorld(localFeatureCoords, this);
        terrain->generateBiomeSpecificTerrainFeature(worldFeatureCoords);
    }

}

void Chunk::destroy()
{
}

void Chunk::create(MiniFaceManager* mini_face_manager) {

      // get chunk x and z so we can get world space coords
    glm::ivec3 chunkCoord = MiniTerrain::ConvertKeyToChunk(key);

    // For loop through all blocks
    for (int x = 0; x < X_WIDTH; x++) {
        for (int z = 0; z < Z_WIDTH; z++) {
            for (int y = 0; y < Y_WIDTH; y++) {

                BlockType myBlockType = this->getBlockAtLocal(x, y, z);

                if (myBlockType != EMPTY && myBlockType != DNE) {

                    BlockType left = this->getBlockAtLocal(x, y, z-1);
                    BlockType right = this->getBlockAtLocal(x, y, z+1);
                    BlockType down = this->getBlockAtLocal(x, y-1, z);
                    BlockType up = this->getBlockAtLocal(x, y+1, z);
                    BlockType back = this->getBlockAtLocal(x-1, y, z);
                    BlockType forward = this->getBlockAtLocal(x+1, y, z);

                    glm::vec4 myColor = getBlockColor(myBlockType);

                    glm::vec4 botLeftCorWorldPos(x + chunkCoord.x*X_WIDTH, y, z + chunkCoord.z*Z_WIDTH, 1);
                    auto allocate_mini_face = [&](const auto& normal)
                    {
                      MiniFace* mini_face = mini_face_manager->AllocateMiniFace();
                      if (mini_face != nullptr)
                      {
                        mini_face->SetFacePos(botLeftCorWorldPos, normal);
                        mini_face->SetTexture(myBlockType);
                      }
                    };

                    // Now for each block face that faces an empty/water block or if the block is water and is not next to another water block input the coords into the vbo
                    if (forward == EMPTY || (forward == WATER && myBlockType != WATER) || (forward == LAVA && myBlockType != LAVA) || (forward == ICE && myBlockType != ICE)) {
                        glm::vec4 vecNor = glm::vec4(1, 0, 0, 0); // normal is positive x-direction
                        allocate_mini_face(vecNor);
                    }
                    if (back == EMPTY || (back == WATER && myBlockType != WATER) || (back == LAVA && myBlockType != LAVA) || (back == ICE && myBlockType != ICE)) 
                    {
                        glm::vec4 vecNor = glm::vec4(-1, 0, 0, 0); // normal is neg x-direction
                        allocate_mini_face(vecNor);
                    }
                    if (left == EMPTY || (left == WATER && myBlockType != WATER) || (left == LAVA && myBlockType != LAVA) || (left == ICE && myBlockType != ICE)) {
                        glm::vec4 vecNor = glm::vec4(0, 0, -1, 0); // normal is neg z-direction
                        allocate_mini_face(vecNor);
                    }
                    if (right == EMPTY || (right == WATER && myBlockType != WATER) || (right == LAVA && myBlockType != LAVA) || (right == ICE && myBlockType != ICE)) {
                        glm::vec4 vecNor = glm::vec4(0, 0, 1, 0); // normal is positive z-direction
                        allocate_mini_face(vecNor);
                    }
                    if (up == EMPTY || (up == WATER && myBlockType != WATER) || (up == LAVA && myBlockType != LAVA) || (up == ICE && myBlockType != ICE)) {
                        glm::vec4 vecNor = glm::vec4(0, 1, 0, 0); // normal is positive y-direction
                        allocate_mini_face(vecNor);
                    }
                    if (down == EMPTY || (down == WATER && myBlockType != WATER) || (down == LAVA && myBlockType != LAVA) || (down == ICE && myBlockType != ICE)) {
                        glm::vec4 vecNor = glm::vec4(0, -1, 0, 0); // normal is neg y-direction
                        allocate_mini_face(vecNor);
                    }
                }
            }
        }
    }
}

BlockType Chunk::getBlockAtLocal(int x, int y, int z) const {

    /* If the block is not in this chunk recurse on the correct chunk */
    if ((z >= Z_WIDTH) || (z < 0) || (x >= X_WIDTH) || (x < 0)) {
        glm::ivec3 world = MiniTerrain::ConvertChunkLocalToWorld(glm::ivec3(x,y,z), this);
        uint64_t adjChunkKey = MiniTerrain::ConvertChunkToKey(MiniTerrain::ConvertWorldToChunk(world));
        bool exists = terrain->chunkMap.find(adjChunkKey) != std::end(terrain->chunkMap);
        if (exists) {
            Chunk* adjacentChunk = terrain->chunkMap[adjChunkKey];
            glm::ivec3 newLocal = MiniTerrain::ConvertWorldToChunkLocal(world);
            return adjacentChunk->getBlockAtLocal(newLocal.x, newLocal.y, newLocal.z);
        } else {
            return DNE;
        }
    /* If block is below the world, return DNE */
    } else if (y >= Y_WIDTH || y < 0) {
        return DNE;

    /* If block is in this chunk, index into the array */
    } else {
        return blocks[x*Z_WIDTH*Y_WIDTH + z*Y_WIDTH + y];
    }

}

BlockType& Chunk::getBlockRefAtLocal(int x, int y, int z) {

    /* If the block is not in this chunk recurse on the correct chunk */
    if ((z >= Z_WIDTH) || (z < 0) || (x >= X_WIDTH) || (x < 0)) {
        glm::ivec3 world = MiniTerrain::ConvertChunkLocalToWorld(glm::ivec3(x,y,z), this);
        uint64_t adjChunkKey = MiniTerrain::ConvertChunkToKey(MiniTerrain::ConvertWorldToChunk(world));
        if (terrain->chunkMap.find(adjChunkKey) != std::end(terrain->chunkMap)) {
            Chunk* adjacentChunk = terrain->chunkMap[adjChunkKey];
            glm::ivec3 newLocal = MiniTerrain::ConvertWorldToChunkLocal(world);
            return adjacentChunk->getBlockRefAtLocal(newLocal.x, newLocal.y, newLocal.z);
        } else {
            std::cerr << "Error: getBlockRefAtLocal--Adjacent Chunk DNE" << std::endl;
            return blocks[0];
        }

        /* If block is out of bound in Y direction, return EMPTY */
    } else if (y >= Y_WIDTH || y < 0) {
        //std::cerr << "Error: getBlockRefAtLocal--Invalid Y" << std::endl;
        return blocks[0];

        /* If block is in this chunk, index into the array */
    } else {
        return blocks[x*Z_WIDTH*Y_WIDTH + z*Y_WIDTH + y];
    }

}

glm::vec4 Chunk::getBlockColor(BlockType b) {
    switch (b) {
    case DIRT :
        return glm::vec4(102, 51, 0, 1) / 255.0f;
    case STONE :
        return glm::vec4(128, 128, 128, 1) / 255.0f;
    case GRASS :
        return glm::vec4(0, 204, 0, 1) / 255.0f;
    case LAVA :
        return glm::vec4(255, 0, 0, 1) / 255.0f;
    default :
        return glm::vec4(0, 0, 0, 0);
    }
}

glm::vec2 Chunk::getTopLeftUV(BlockType b, glm::vec4 nor, std::vector<float> *pows) {
    //map each combo of blocktype and face to top left corner of texture block

    if(b == GRASS) {
        //top face
        if(glm::normalize(nor) == glm::vec4(0, 1, 0, 0)) {
            pows->push_back(100.0f);
            pows->push_back(100.0f);
            pows->push_back(100.0f);
            pows->push_back(100.0f);
            return glm::vec2(8.0f/16.0f, 2.0f/16.0f);
        }
        //bottom face
        else if (glm::normalize(nor) == glm::vec4(0, -1, 0, 0)) {
            pows->push_back(100.0f);
            pows->push_back(100.0f);
            pows->push_back(100.0f);
            pows->push_back(100.0f);
            return glm::vec2(2.0f/16.0f, 0.0f/16.0f);
        }
        //side faces
        else {
            pows->push_back(100.0f);
            pows->push_back(100.0f);
            pows->push_back(100.0f);
            pows->push_back(100.0f);
            return glm::vec2(3.0f/16.0f, 0.0f/16.0f);

        }
    }
    else if(b == STONE) {
        pows->push_back(140.0f);
        pows->push_back(140.0f);
        pows->push_back(140.0f);
        pows->push_back(140.0f);
        return glm::vec2(1.0f/16.0f, 0.0f/16.0f);
    }
    else if(b == WOOD) {
        pows->push_back(140.0f);
        pows->push_back(140.0f);
        pows->push_back(140.0f);
        pows->push_back(140.0f);
        if(glm::normalize(nor) == glm::vec4(0, 1, 0, 0) || glm::normalize(nor) == glm::vec4(0, -1, 0, 0)) {
            return glm::vec2(5.0f/16.0f, 1.0f/16.0f);
        }
        else {
            return glm::vec2(4.0f/16.0f, 1.0f/16.0f);
        }
    }
    else if(b == BEDROCK) {
        pows->push_back(170.0f);
        pows->push_back(170.0f);
        pows->push_back(170.0f);
        pows->push_back(170.0f);
        return glm::vec2(1.0f/16.0f, 1.0f/16.0f);
    }
    else if(b == DIRT) {
        pows->push_back(100.0f);
        pows->push_back(100.0f);
        pows->push_back(100.0f);
        pows->push_back(100.0f);
        return glm::vec2(2.0f/16.0f, 0.0f);
    }
    else if(b == LEAF) {
        pows->push_back(100.0f);
        pows->push_back(100.0f);
        pows->push_back(100.0f);
        pows->push_back(100.0f);
        return glm::vec2(5.0f/16.0f, 3.0f/16.0f);
    }
    else if(b == ICE) {
        pows->push_back(150.0f);
        pows->push_back(150.0f);
        pows->push_back(150.0f);
        pows->push_back(150.0f);
        return glm::vec2(3.0f/16.0f, 4.0f/16.0f);
    }
    else if(b == WATER) {
        pows->push_back(120.0f);
        pows->push_back(120.0f);
        pows->push_back(120.0f);
        pows->push_back(120.0f);
        return glm::vec2(13.0f/16.0f, 12.0f/16.0f);
    }
    else if(b == LAVA) {
        pows->push_back(130.0f);
        pows->push_back(130.0f);
        pows->push_back(130.0f);
        pows->push_back(130.0f);
        return glm::vec2(13.0f/16.0f, 14.0f/16.0f);
    }
    else if(b == SAND) {
        pows->push_back(130.0f);
        pows->push_back(130.0f);
        pows->push_back(130.0f);
        pows->push_back(130.0f);
        return glm::vec2(2.0f/16.0f, 1.f/16.0f);
    }
    else if (b == COAL) {
        pows->push_back(170.0f);
        pows->push_back(170.0f);
        pows->push_back(170.0f);
        pows->push_back(170.0f);
        return glm::vec2(2.0f/16.0f, 2.f/16.0f);
    }
    else if (b == DIAMOND_ORE) {
        pows->push_back(170.0f);
        pows->push_back(170.0f);
        pows->push_back(170.0f);
        pows->push_back(170.0f);
        return glm::vec2(2.0f/16.0f, 3.0f/16.0f);
    }
    else if (b == IRON_ORE) {
        pows->push_back(170.0f);
        pows->push_back(170.0f);
        pows->push_back(170.0f);
        pows->push_back(170.0f);
        return glm::vec2(1.0f/16.0f, 2.0f/16.0f);
    } else if (b == REDSTONE_ORE) {
        pows->push_back(170.0f);
        pows->push_back(170.0f);
        pows->push_back(170.0f);
        pows->push_back(170.0f);
        return glm::vec2(3.0f/16.0f, 3.0f/16.0f);
    } else if (b == GOLD_ORE) {
        pows->push_back(170.0f);
        pows->push_back(170.0f);
        pows->push_back(170.0f);
        pows->push_back(170.0f);
        return glm::vec2(0.0f/16.0f, 2.0f/16.0f);
    } else if (b == CACTUS) {
        pows->push_back(170.0f);
        pows->push_back(170.0f);
        pows->push_back(170.0f);
        pows->push_back(170.0f);
        return glm::vec2(6.0f/16.0f, 4.0f/16.0f);
    } else if (b == SNOW) {
        pows->push_back(170.0f);
        pows->push_back(170.0f);
        pows->push_back(170.0f);
        pows->push_back(170.0f);
        return glm::vec2(2.0f/16.0f, 4.0f/16.0f);
    }
    else {
        std::cerr << "Top Left UV - Type of Block not added!" << std::endl;
        return glm::vec2(0,0);
    }
}

void Chunk::getFaceUVs(glm::vec2 topLeft, std::vector<glm::vec2> *uvs) {
    //bottom left, bottom right, top right, top left
    uvs->push_back(glm::vec2(topLeft[0], topLeft[1] + 1.0f/16.0f));
    uvs->push_back(glm::vec2(topLeft[0] + 1.0f/16.0f, topLeft[1] + 1.0f/16.0f));
    uvs->push_back(glm::vec2(topLeft[0] + 1.0f/16.0f, topLeft[1]));
    uvs->push_back(topLeft);
}

//function used to interpolate heights between biomes. Needs to be improved (do not use it)
void MiniTerrain::fillColumn(BlockType b1, BlockType b2, BlockType b3, glm::vec2 colPos,
                         float colHeightPercent, float p1, float p2, float p3) {
    int h1;
    int h2;
    int h3;

    switch (b2) {
    case GRASS:
        h2 = 128 + colHeightPercent * 80;
        break;
    case SAND:
        h2 = 128 + colHeightPercent * 80;
        break;
    case SNOW:
        h2 = 128 + colHeightPercent * 128;
        break;
    case WATER:
        h2 = 100 + colHeightPercent * 50;
        break;
    default:
        break;
    }

    switch (b3) {
    case GRASS:
        h3 = 128 + colHeightPercent * 80;
        break;
    case SAND:
        h2 = 128 + colHeightPercent * 80;
        break;
    case SNOW:
        h3 = 128 + colHeightPercent * 128;
        break;
    case WATER:
        h3 = 100 + colHeightPercent * 50;
        break;
    default:
        break;
    }


    switch (b1) {
    case SAND:
        h1 = 128 + colHeightPercent * 80;
        h1 = p1 * h1 + p2 * h2 + p3 * h3;
        for (int y = 0; y < 256; ++y) {
            if (y < 127) {
                setBlockAt(colPos.x, y, colPos.y, STONE);
            } else if (y < h1) {
                setBlockAt(colPos.x, y, colPos.y, SAND);
            } else if (y == h1) {
                setBlockAt(colPos.x, y, colPos.y, SAND);
            } else {
                setBlockAt(colPos.x, y, colPos.y, EMPTY);
            }
        }
        break;
    case GRASS:
        h1 = 128 + colHeightPercent * 80;
        h1 = p1 * h1 + p2 * h2 + p3 * h3;
        for (int y = 0; y < 256; ++y) {
            if (y < 127) {
                setBlockAt(colPos.x, y, colPos.y, STONE);
            } else if (y < h1) {
                setBlockAt(colPos.x, y, colPos.y, DIRT);
            } else if (y == h1) {
                setBlockAt(colPos.x, y, colPos.y, GRASS);
            } else {
                setBlockAt(colPos.x, y, colPos.y, EMPTY);
            }
        }
    case SNOW:
        h1 = 128 + colHeightPercent * 128;
        h1 = p1 * h1 + p2 * h2 + p3 * h3;
        for (int y = 0; y < 256; ++y) {
            if (y < 127) {
                setBlockAt(colPos.x, y, colPos.y, STONE);
            } else if (y < h1) {
                setBlockAt(colPos.x, y, colPos.y, SNOW);
            } else if (y == h1) {

                setBlockAt(colPos.x, y, colPos.y, SNOW);
            } else {
                setBlockAt(colPos.x, y, colPos.y, EMPTY);
            }
        }
        break;
    case WATER:
        h1 = 128 + colHeightPercent * 50;
        h1 = p1 * h1 + p2 * h2 + p3 * h3;
        for (int y = 0; y < 256; ++y) {
            if (y < 100) {
                setBlockAt(colPos.x, y, colPos.y, STONE);
            } else if (y < 110) {
                setBlockAt(colPos.x, y, colPos.y, DIRT);
            } else if (y < h1) {
                setBlockAt(colPos.x, y, colPos.y, WATER);
            } else if (y == h1) {
                setBlockAt(colPos.x, y, colPos.y, WATER);
            } else {
                setBlockAt(colPos.x, y, colPos.y, EMPTY);
            }
        }
        break;
    default:
        break;
    }
}
