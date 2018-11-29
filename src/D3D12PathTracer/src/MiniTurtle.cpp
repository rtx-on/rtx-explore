#include "stdafx.h"
#include "MiniTurtle.h"

// TODO: Possibly refactor this to be an internal class of LSystem and rename LSystem to River (then we can use the private rule functions)

MiniTurtle::MiniTurtle(MiniTurtle* turtle, std::string newGrammar)
    : position(turtle->position), orientation(turtle->orientation), recursionDepth(turtle->recursionDepth),
      system(turtle->system), iter(newGrammar.begin()), grammar(newGrammar)
{}

MiniTurtle::MiniTurtle(MiniTurtle* turtle, std::string::iterator newIter)
    : position(turtle->position), orientation(turtle->orientation), recursionDepth(turtle->recursionDepth),
      system(turtle->system), iter(newIter), grammar(turtle->grammar)
{}

MiniTurtle::MiniTurtle(std::string initialAxiom, MiniLSystem* system, glm::vec3 position, glm::vec3 orientation)
    : position(position), orientation(orientation), recursionDepth(1), system(system), iter(initialAxiom.begin()), grammar(initialAxiom)
{}

glm::vec3 MiniTurtle::getMyPosition() {
    return this->position;
}

std::pair<glm::vec3, glm::vec3> MiniTurtle::executeNextMovementRule() {
    /* store initial position */
    glm::vec3 initialPosition = this->position;

    while (initialPosition == this->position) {
        /* if the turtle has no more characters to read, pop off stack and return empty pair */
        if (this->system->turtleStack.top() != this) {
            return std::pair<glm::vec3, glm::vec3>();
        } else if (iter == grammar.end()) {
            this->system->turtleStack.pop();
            return std::pair<glm::vec3, glm::vec3>();
        }

        /* debug print statement */
        //std::cout << (*iter).toLatin1();

        /* If the character is in the grammar map */
        if (system->charToGrammarExpansion.find(*iter) != std::end(system->charToGrammarExpansion)) {
            /* if we are at maxRecursionDepth, just continue to the next char */
            if (this->recursionDepth <= system->maxRecursionDepth) {
                /* clone yourself */
                MiniTurtle* clone = new MiniTurtle(this, (this->iter + 1));

                /* replace your grammar with the new one and increment recursion depth */
                this->grammar = system->charToGrammarExpansion[*iter];
                this->iter = this->grammar.begin();
                this->recursionDepth++;

                /* reorder stack to be you -> clone -> rest of stack */
                MiniTurtle *self = system->turtleStack.top();
                system->turtleStack.pop();
                if (this != self) {
                    std::cerr << "ExecuteNextMovementRule: this turtle not at top of stack" << std::endl;
                }

                system->turtleStack.push(clone);
                system->turtleStack.push(self);
                continue;
            }
            iter++;
            continue;
        }

        /* If the character is in the rule map, execute the rule */
        if (system->charToDrawingOperation.find(*iter) != std::end(system->charToDrawingOperation)) {
            Rule rule = system->charToDrawingOperation[*iter];
            rule(this);
        }
        iter++;
    }

    std::pair<glm::vec3, glm::vec3> pair(initialPosition, this->position);
    return pair;
}
