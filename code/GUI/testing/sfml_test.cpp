#include <SFML/Graphics.hpp>

int main() {
    // Create a window
    sf::RenderWindow window(sf::VideoMode(800, 600), "SFML Test");

    // Create a circle shape
    sf::CircleShape shape(50.f);
    shape.setFillColor(sf::Color::Green);
    shape.setPosition(375, 275);  // Center the circle in the window

    // Main loop
    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();
        }

        // Clear the screen
        window.clear(sf::Color::Black);

        // Draw the circle
        window.draw(shape);

        // Display the contents of the window
        window.display();
    }

    return 0;
}

//g++ -o sfml_test sfml_test.cpp -lsfml-graphics -lsfml-window -lsfml-system