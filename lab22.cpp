#include <iostream>
#include <thread>
#include <semaphore>
#include <queue>
#include <mutex>
#include <atomic>
#include <vector>
#include <random>
#include <chrono>

const int NUM_CAMERAS = 6;
const int NUM_ACCELERATORS = 3; 
const int EMERGENCY_PRIORITY = 0; 
const int NORMAL_PRIORITY = 1;    

struct FrameTask {
    int camera_id;
    int frame_id;
    int priority; 
};

std::priority_queue<FrameTask, std::vector<FrameTask>, 
    bool(*)(const FrameTask&, const FrameTask&)> task_queue(
        [](const FrameTask& a, const FrameTask& b) {
            return a.priority > b.priority; 
        }
    );

std::mutex queue_mutex;                   
std::counting_semaphore<NUM_ACCELERATORS> accelerators(NUM_ACCELERATORS); 
std::atomic<bool> accelerators_active[NUM_ACCELERATORS] = {true, true, true}; 
std::atomic<int> total_load(0);    

void process_frame(int accelerator_id, const FrameTask& task) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> processing_time(1, 3); 

    int time = processing_time(gen);
    std::this_thread::sleep_for(std::chrono::seconds(time));

    std::lock_guard<std::mutex> lock(queue_mutex);
    std::cout << "Ускоритель " << accelerator_id 
              << " обработал кадр " << task.frame_id 
              << " (камера " << task.camera_id << ")"
              << " [Приоритет: " << (task.priority == EMERGENCY_PRIORITY ? "Важный" : "Обычный") << "]\n";
}

void monitor_load() {
    while (true) {
        int load = total_load.load();
        if (load > 80) { 
            accelerators.release();
            std::cout << "Добавлен резервный ускоритель!\n";
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}


void camera_stream(int camera_id) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> frame_id_gen(1, 1000);
    int frame_counter = 0;

    while (true) {
        FrameTask task;
        task.camera_id = camera_id;
        task.frame_id = frame_id_gen(gen);
        task.priority = (task.frame_id % 2 == 0) ? EMERGENCY_PRIORITY : NORMAL_PRIORITY;

        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            task_queue.push(task);
            std::cout << "Камера " << camera_id << " отправила кадр " << task.frame_id 
                      << " [Приоритет: " << (task.priority == EMERGENCY_PRIORITY ? "Важный" : "Обычный") << "]\n";
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500)); 
        frame_counter++;
    }
}

void accelerator_worker() {
    while (true) {
        FrameTask task;
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            if (task_queue.empty()) continue;
            task = task_queue.top();
            task_queue.pop();
        }

        accelerators.acquire(); 
        total_load += 20; 

        int accelerator_id = -1;
        for (int i = 0; i < NUM_ACCELERATORS; ++i) {
            if (accelerators_active[i]) {
                accelerator_id = i;
                break;
            }
        }

        if (accelerator_id == -1) {
            accelerators_active[0] = true;
            accelerator_id = 0;
        }

        process_frame(accelerator_id, task);
        total_load -= 20;
        accelerators.release();
    }
}

int main() {
    std::thread monitor(monitor_load);
    monitor.detach();

    std::vector<std::thread> cameras;
    for (int i = 0; i < NUM_CAMERAS; ++i) {
        cameras.emplace_back(camera_stream, i + 1);
    }

    std::vector<std::thread> workers;
    for (int i = 0; i < NUM_ACCELERATORS; ++i) {
        workers.emplace_back(accelerator_worker);
    }

    for (auto& cam : cameras) cam.join();
    for (auto& worker : workers) worker.join();

    return 0;
}
