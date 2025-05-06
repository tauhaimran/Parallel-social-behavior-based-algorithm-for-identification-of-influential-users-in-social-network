#include <iostream>
#include <omp.h>
#include <mpi.h>
#include <SFML/Graphics.hpp>

int main(int argc, char** argv) {
    // --- MPI Init
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    if (rank == 0) std::cout << "MPI: Hello from process 0 of " << size << std::endl;

    // --- OpenMP Parallel Section
    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        #pragma omp critical
        std::cout << "OpenMP: Thread " << tid << " says hello\n";
    }

    // --- SFML Window
    if (rank == 0) {
        sf::RenderWindow window(sf::VideoMode(400, 300), "SFML Test");
        while (window.isOpen()) {
            sf::Event event;
            while (window.pollEvent(event))
                if (event.type == sf::Event::Closed)
                    window.close();

            window.clear(sf::Color::Black);
            sf::CircleShape shape(50.f);
            shape.setFillColor(sf::Color::Green);
            shape.setPosition(100, 100);
            window.draw(shape);
            window.display();
        }
    }

    MPI_Finalize();
    return 0;
}
// Compile with: mpicxx -fopenmp -o main main.cpp -lsfml-graphics -lsfml-window -lsfml-system
// Run with: mpirun -np 4 ./main

//or
//compile: g++ -fopenmp -o combined_test combined_test.cpp -lsfml-graphics -lsfml-window -lsfml-system -lmpi
//run: mpirun -np 4 ./combined_test
// This code combines MPI, OpenMP, and SFML to demonstrate a simple parallel program.
#include <SFML/Graphics.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <thread>

class PipelineGUI {
private:
    sf::RenderWindow window;
    sf::Font font;
    std::vector<sf::RectangleShape> step_boxes;
    std::vector<sf::Text> step_texts;
    sf::Text log_text;
    sf::Text metrics_text;
    sf::Text status_text;
    sf::Text user_info_text;
    sf::Text node_info_text;
    sf::Text title_text;

    std::vector<std::string> pipeline_steps = {
        "Load Data", "Initialize Graph", "Detect Communities",
        "Calculate Influence Power", "Select Seed Candidates", 
        "Select Seeds", "Verify & Log"
    };

    std::vector<bool> step_selected;
    std::vector<std::string> log_lines;

    // For nodes
    std::vector<sf::CircleShape> nodes;
    std::vector<std::string> node_data = {
        "Node 1: Selected Seed", "Node 2: Selected Seed", "Node 3: Unselected"
    };

    bool show_node_screen = false;

    // Scrollable area for log
    sf::RectangleShape log_scrollbar;
    sf::View log_view;

public:
    PipelineGUI() : window(sf::VideoMode(900, 600), "PSAIIM Pipeline Visualizer") {
        if (!font.loadFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf")) {
            std::cerr << "Failed to load font" << std::endl;
        }

        setup_ui();
        load_log_file("graph_analysis.log");
    }

    void setup_ui() {
        step_selected.resize(pipeline_steps.size(), false);

        for (size_t i = 0; i < pipeline_steps.size(); i++) {
            sf::RectangleShape box(sf::Vector2f(200, 40));
            box.setPosition(20, 20 + i * 50);
            box.setFillColor(sf::Color::White);
            box.setOutlineThickness(2);
            box.setOutlineColor(sf::Color::Black);
            step_boxes.push_back(box);

            sf::Text text(pipeline_steps[i], font, 14);
            text.setPosition(30, 25 + i * 50);
            text.setFillColor(sf::Color::Black);
            step_texts.push_back(text);
        }

        log_text.setFont(font);
        log_text.setCharacterSize(12);
        log_text.setPosition(250, 20);
        log_text.setFillColor(sf::Color::White);

        metrics_text.setFont(font);
        metrics_text.setCharacterSize(14);
        metrics_text.setPosition(250, 400);
        metrics_text.setFillColor(sf::Color::White);

        status_text.setFont(font);
        status_text.setCharacterSize(14);
        status_text.setPosition(250, 460);
        status_text.setFillColor(sf::Color::White);

        user_info_text.setFont(font);
        user_info_text.setCharacterSize(14);
        user_info_text.setPosition(50, 500);
        user_info_text.setFillColor(sf::Color::White);
        user_info_text.setString("Made by: \n Tahir\nSameed\nTauha");

        node_info_text.setFont(font);
        node_info_text.setCharacterSize(14);
        node_info_text.setPosition(250, 520);
        node_info_text.setFillColor(sf::Color::White);

        for (size_t i = 0; i < node_data.size(); i++) {
            sf::CircleShape node(20);
            node.setPosition(100 + i * 100, 200);
            node.setFillColor(i < 2 ? sf::Color::Green : sf::Color::Red);
            nodes.push_back(node);
        }

        log_scrollbar.setSize(sf::Vector2f(10, 100));
        log_scrollbar.setPosition(860, 20);
        log_scrollbar.setFillColor(sf::Color::Black);
    }

    void load_log_file(const std::string& filename) {
        std::ifstream file(filename);
        std::string line;
        log_lines.clear();

        while (std::getline(file, line)) {
            log_lines.push_back(line);
        }

        update_log_display();
    }

    void update_log_display() {
        std::string display_text;
        size_t start = log_lines.size() > 50 ? log_lines.size() - 50 : 0;
        for (size_t i = start; i < log_lines.size(); i++) {
            display_text += log_lines[i] + "\n";
        }
        log_text.setString(display_text);
    }

    void run() {
        while (window.isOpen()) {
            sf::Event event;
            while (window.pollEvent(event)) {
                if (event.type == sf::Event::Closed) {
                    window.close();
                } else if (event.type == sf::Event::KeyPressed) {
                    if (event.key.code == sf::Keyboard::N) {
                        show_node_screen = true;
                    } else if (event.key.code == sf::Keyboard::B) {
                        show_node_screen = false;
                    }
                } else if (event.type == sf::Event::MouseButtonPressed) {
                    handle_click(event.mouseButton.x, event.mouseButton.y);
                }
            }

            window.clear(sf::Color(50, 50, 50));
            if (show_node_screen)
                draw_node_screen();
            else
                draw_main_screen();
            window.display();
        }
    }

    void draw_main_screen() {
        for (size_t i = 0; i < step_boxes.size(); i++) {
            window.draw(step_boxes[i]);
            window.draw(step_texts[i]);
        }

        window.draw(log_text);
        window.draw(metrics_text);
        window.draw(status_text);
        window.draw(user_info_text);
        window.draw(node_info_text);

        // Scrollable log area
        window.draw(log_scrollbar);
    }

    void draw_node_screen() {
        sf::Text title("Node Visualization - Press 'B' to go back", font, 16);
        title.setPosition(20, 10);
        title.setFillColor(sf::Color::White);
        window.draw(title);

        for (size_t i = 0; i < nodes.size(); i++) {
            window.draw(nodes[i]);

            sf::Text label(node_data[i], font, 12);
            label.setPosition(nodes[i].getPosition().x - 10, nodes[i].getPosition().y + 40);
            label.setFillColor(sf::Color::White);
            window.draw(label);
        }
    }

    void handle_click(int x, int y) {
        if (!show_node_screen) {
            for (size_t i = 0; i < step_boxes.size(); i++) {
                if (step_boxes[i].getGlobalBounds().contains(x, y)) {
                    step_selected[i] = !step_selected[i];
                    step_boxes[i].setFillColor(step_selected[i] ? sf::Color::Green : sf::Color::White);
                }
            }
        } else {
            for (size_t i = 0; i < nodes.size(); i++) {
                if (nodes[i].getGlobalBounds().contains(x, y)) {
                    node_info_text.setString(node_data[i]);
                }
            }
        }
    }

    void execute_selected_steps() {
        auto start = std::chrono::high_resolution_clock::now();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::string metrics = "Execution Time: " + std::to_string(duration.count()) + "ms\n";
        metrics_text.setString(metrics);
        status_text.setString("Status: Running");
    }
};

int main() {
    PipelineGUI gui;
    gui.run();
    return 0;
}
