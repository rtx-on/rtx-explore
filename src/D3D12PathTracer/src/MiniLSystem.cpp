#include "stdafx.h"
#include "MiniLSystem.h"

MiniLSystem::MiniLSystem(RiverType type, glm::vec3 position, glm::vec3 orientation)
    : turtleStack(), charToGrammarExpansion(), charToDrawingOperation(),
      maxRecursionDepth(type == CAVE ? CAVE_MAX_RECURSION_DEPTH : MAX_RECURSION_DEPTH),
      recursionDropFactor(type == CAVE ? CAVE_RECURSION_DROP_FACTOR : RECURSION_DROP_FACTOR)
{

    /* initialize the grammar maps and drawing operations of the MiniLSystem */
    initLSystemMaps(type);

    /* get the hardcoded initial axiom for the river type */
    std::string initialAxiom = getInitialAxiom(type);

    /* push the turtle with the initial position of the MiniLSystem */
    MiniTurtle* initialTurtle = new MiniTurtle(initialAxiom, this, position, orientation);
    turtleStack.push(initialTurtle);
}

void MiniLSystem::ruleMoveForward(MiniTurtle* turtle) {
    float noise = (float) rand()/ (float) RAND_MAX;
     if (noise < WAVINESS) {
         ruleTurnLeft(turtle);
     } else if (noise > 1 - WAVINESS) {
         ruleTurnRight(turtle);
     }
     turtle->position += turtle->orientation * ((float) turtle->system->recursionDropFactor) * FORWARD_DISTANCE;
}

void MiniLSystem::ruleTurnLeft(MiniTurtle* turtle) {
     glm::mat4 rotMat = glm::rotate(glm::mat4(1.0f), glm::radians(15.0f), glm::vec3(0,1,0));
     turtle->orientation = glm::vec3(glm::normalize(rotMat * glm::vec4(turtle->orientation, 0)));
}

void MiniLSystem::ruleTurnRight(MiniTurtle* turtle) {
    glm::mat4 rotMat = glm::rotate(glm::mat4(1.0f), glm::radians(-15.0f), glm::vec3(0,1,0));
    turtle->orientation = glm::vec3(glm::normalize(rotMat * glm::vec4(turtle->orientation, 0)));
}

void MiniLSystem::ruleTurnAround(MiniTurtle* turtle) {
    float noise = (float) rand()/ (float) RAND_MAX;
    if (noise < WAVINESS) {
        for (float i = 0; i < noise; i += 0.1f) {
            ruleTurnLeft(turtle);
        }
    } else if (noise > 1 - WAVINESS) {
        for (float i = 0; i < 1 - noise; i += 0.1f) {
            ruleTurnRight(turtle);
        }
    }
    turtle->orientation = glm::normalize(glm::vec3(turtle->orientation.x * -1.0f, turtle->orientation.y, turtle->orientation.z * -1.0f));
}

void MiniLSystem::ruleRecurse(MiniTurtle* turtle) {

    /* find the next ] token */
    std::string::iterator newIter = turtle->iter;
    while (((*newIter) != ']') && (newIter < turtle->grammar.end())) {
        newIter++;
    }

    if ((*newIter) != ']') {
        std::cerr << "Error: rule recurse, rule has [ but no ]" << std::endl;
    } else {
        /* pop yourself off */
        MiniTurtle *me = turtle->system->turtleStack.top(); 
        turtle->system->turtleStack.pop();
        if (me != turtle) {
            std::cerr << "Error: rule recurse, turtle at top of stack isn't me" << std::endl;
        }

        /* push your clone (at position ]) on and then yourself back on */
        turtle->system->turtleStack.push(new MiniTurtle(turtle, (newIter + 1)));
        turtle->system->turtleStack.push(me);
        //turtle->recursionDepth++;
    }
}

int MiniLSystem::getCurrentRecursionDepth() {
    if (turtleStack.empty()) {
        return 0;
    } else {
        return turtleStack.top()->recursionDepth;
    }
}

void MiniLSystem::rulePop(MiniTurtle* turtle) {
    turtle->system->turtleStack.pop();
}

void MiniLSystem::initLSystemMaps(RiverType type)
{
    switch (type) {
        case LINEAR:
            populateGrammarMapLinear();
            populateRuleMapLinear();
            break;
        case DELTA:
            populateGrammarMapDelta();
            populateRuleMapDelta();
            break;
        case CAVE:
            populateGrammarMapCave();
            populateRuleMapCave();
            break;
    }
}

std::string MiniLSystem::getInitialAxiom(RiverType type)
{
    switch (type) {
        case LINEAR:
            return "FX";
        case DELTA:
            return "FX";
        case CAVE:
            return "FX";
        default:
            std::cerr << "Error: getInitialAxiom invalid RiverType input" << std::endl;
            return "";
    }
}

void MiniLSystem::populateGrammarMapLinear()
{
    charToGrammarExpansion.insert({'X', "[Y]F[W]FX"});
    charToGrammarExpansion.insert({'Y', "-F+F+F"}); /* branch left & come back*/
    charToGrammarExpansion.insert({'W', "+F-F-F"}); /* branch right & come back*/
}

void MiniLSystem::populateRuleMapLinear()
{
    charToDrawingOperation.insert({'F', &ruleMoveForward});
    charToDrawingOperation.insert({'-', &ruleTurnLeft});
    charToDrawingOperation.insert({'+', &ruleTurnRight});
    charToDrawingOperation.insert({'[', &ruleRecurse});
    charToDrawingOperation.insert({']', &rulePop});
}

void MiniLSystem::populateGrammarMapDelta()
{
    charToGrammarExpansion.insert({'X', "[-FX]+FX"});
}

void MiniLSystem::populateRuleMapDelta()
{
    charToDrawingOperation.insert({'F', &ruleMoveForward});
    charToDrawingOperation.insert({'-', &ruleTurnLeft});
    charToDrawingOperation.insert({'+', &ruleTurnRight});
    charToDrawingOperation.insert({'[', &ruleRecurse});
    charToDrawingOperation.insert({']', &rulePop});
}

void MiniLSystem::populateGrammarMapCave()
{
    charToGrammarExpansion.insert({'X', "[Y]F[W]F<X"});
    charToGrammarExpansion.insert({'Y', "-F+F+F"}); /* branch left & come back*/
    charToGrammarExpansion.insert({'W', "+F-F-F"}); /* branch right & come back*/
}

void MiniLSystem::populateRuleMapCave()
{
    charToDrawingOperation.insert({'F', &ruleMoveForward});
    charToDrawingOperation.insert({'-', &ruleTurnLeft});
    charToDrawingOperation.insert({'+', &ruleTurnRight});
    charToDrawingOperation.insert({'<', &ruleTurnAround});
    charToDrawingOperation.insert({'[', &ruleRecurse});
    charToDrawingOperation.insert({']', &rulePop});
}

std::pair<glm::vec3, glm::vec3> MiniLSystem::getNextLine()
{
    std::pair<glm::vec3, glm::vec3> emptyPair;
    while(!turtleStack.empty()) {
        std::pair<glm::vec3, glm::vec3> pair = turtleStack.top()->executeNextMovementRule();
        if (pair != emptyPair) return pair;
    }
    return emptyPair;
}
