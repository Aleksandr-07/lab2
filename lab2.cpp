#include <iostream>
#include <thread>
#include <queue>
#include <mutex>
#include <semaphore>
#include <random>
#include <chrono>
#include <atomic>

std::counting_semaphore<10> cranes(5); 
std::queue<int> truck_queue;
std::mutex queue_mutex;
std::mutex cout_mutex;
std::atomic<int> loaded_trucks(0);
std::atomic<bool> emergency_mode(false);

void truck(int id) {
    cranes.acquire(); 

    {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "Грузовик " << id << " начал загрузку.\n";
    }

    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> time_dist(3, 6);
    int load_time = emergency_mode ? time_dist(gen) / 2 : time_dist(gen);

    std::this_thread::sleep_for(std::chrono::seconds(load_time));

    {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "Грузовик " << id << " завершил загрузку за " << load_time << " сек.\n";
    }

    loaded_trucks++; 
    cranes.release(); 
}

void monitor() {
    while (loaded_trucks < 10) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            if (truck_queue.size() > 5 && cranes.try_acquire()) {
                cranes.release(); 
                cranes.release(); 
                std::lock_guard<std::mutex> lock_cout(cout_mutex);
                std::cout << "Активирован резервный кран! Доступно: " << 10 - cranes.try_acquire() << "\n";
                cranes.release();
            }
        }

        if (loaded_trucks.load() < 3 && !emergency_mode) {
            emergency_mode = true;
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "Аварийная загрузка активирована! (Загружено: " << loaded_trucks << ")\n";
        } else if (loaded_trucks.load() >= 3 && emergency_mode) {
            emergency_mode = false;
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "Аварийная загрузка отключена (Загружено: " << loaded_trucks << ")\n";
        }
    }
}

int main() {
    std::thread monitoring_thread(monitor);

    std::vector<std::thread> trucks;
    for (int i = 1; i <= 10; ++i) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            truck_queue.push(i);
        }
        trucks.emplace_back(truck, i);
    }

    for (auto& t : trucks) t.join();
    monitoring_thread.join();

    return 0;
}
