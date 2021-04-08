#include <iostream>
#include <string>
#include <list>
#include <vector>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <unistd.h>
#include <ctime>
#include "configreader.h"
#include "process.h"

// Shared data for all cores
typedef struct SchedulerData {
    std::mutex mutex;
    std::condition_variable condition;
    ScheduleAlgorithm algorithm;
    uint32_t context_switch;
    uint32_t time_slice;
    std::list<Process*> ready_queue;
    bool all_terminated;
    std::vector<double> turnTimes;
} SchedulerData;

void coreRunProcesses(uint8_t core_id, SchedulerData *data);
int printProcessOutput(std::vector<Process*>& processes, std::mutex& mutex);
void clearOutput(int num_lines);
uint64_t currentTime();
std::string processStateToString(Process::State state);
uint64_t start = currentTime();


int main(int argc, char **argv)
{
    //std::cout << "Hello world";
    // Ensure user entered a command line parameter for configuration file name
    if (argc < 2)
    {
        std::cerr << "Error: must specify configuration file" << std::endl;
        exit(EXIT_FAILURE);
    }

    // Declare variables used throughout main
    int i;
    SchedulerData *shared_data;
    std::vector<Process*> processes;

    // Read configuration file for scheduling simulation
    SchedulerConfig *config = readConfigFile(argv[1]);

    // Store configuration parameters in shared data object
    uint8_t num_cores = config->cores;
    shared_data = new SchedulerData();
    shared_data->algorithm = config->algorithm;
    shared_data->context_switch = config->context_switch;
    shared_data->time_slice = config->time_slice;
    shared_data->all_terminated = false;

    // Create processes
    for (i = 0; i < config->num_processes; i++)
    {
        
        Process *p = new Process(config->processes[i], start);
        processes.push_back(p);
        // If process should be launched immediately, add to ready queue
        if (p->getState() == Process::State::Ready)
        {
            shared_data->ready_queue.push_back(p);
        }
        
    }

    

    //std::cout << "num proc " << shared_data->ready_queue.size() << "\n";
    // Free configuration data from memory
    deleteConfig(config);

    // Launch 1 scheduling thread per cpu core
    std::thread *schedule_threads = new std::thread[num_cores];
    for (i = 0; i < num_cores; i++)
    {
        schedule_threads[i] = std::thread(coreRunProcesses, i, shared_data);
    }

    /*//sort ready queue before main work
    if(shared_data->algorithm == SJF) {
        for(int i = 0; i < shared_data->ready_queue.size(); i++) {
            for(int j = shared_data->ready_queue.size() - 1; j > i; j--) {
                if(shared_data->ready_queue.at(j)->getRemainingTime() < shared_data->ready_queue.at(i)->getRemainingTime()) {
                    Process *holder = shared_data->ready_queue.at(i);
                    shared_data->ready_queue.at(i) = shared_data->ready_queue.at(j);
                    shared_data->ready_queue.at(j) = holder;
                }
            }
        }
    }*/
    SjfComparator SJFoperate;
    PpComparator PPoperate;

    if(shared_data->algorithm == SJF) {
        shared_data->ready_queue.sort(SJFoperate);
    } else if(shared_data->algorithm == PP) {
        shared_data->ready_queue.sort(PPoperate);
    }
    
    
    //print out ready queue remaining time
    /*std::list<Process>::iterator it;
    for(auto const& i : shared_data->ready_queue) {
        std::cout << i->getRemainingTime() << std::endl;
    }*/

    // Main thread work goes here
    int num_lines = 0;
    while (!(shared_data->all_terminated))
    {

        // Clear output from previous iteration
        clearOutput(num_lines);
        //std::cout << "num proc " << shared_data->ready_queue.size() << "\n";
        //std::cout << "num proc " << processes.size() << "\n";
        // Do the following:

        //   - Get current time
        uint64_t current_time = currentTime();


        for(int i = 0; i < processes.size(); i++) {
            if(processes.at(i)->getState() == Process::State::IO) {
                //usleep(1000 * processes.at(i)->getBurstTime(processes.at(i)->getBurstIdx()));
                if(/*io burst time elapsed*/ currentTime() - processes.at(i)->getIOStartTime() >= processes.at(i)->getBurstTime(processes.at(i)->getBurstIdx())) {
                    processes.at(i)->setState(Process::State::Ready, currentTime());
                    processes.at(i)->updateBurstIdx();
                    
                    //Here will be the code for where to put it in the ready queue 
                    
                    shared_data->mutex.lock();
                    if(shared_data->algorithm == FCFS) {
                        shared_data->ready_queue.push_front(processes.at(i));
                    } else {
                        shared_data->ready_queue.push_back(processes.at(i));
                    }
                    
                    if(shared_data->algorithm == SJF) {
                        //std::cout << "Hello\n";
                        shared_data->ready_queue.sort(SJFoperate);
                    } else if (shared_data->algorithm == PP) {
                        
                        shared_data->ready_queue.sort(PPoperate);
                    }
                    
                    shared_data->mutex.unlock();
                    
                    //std::cout << thisProcess->getBurstIdx();
                }
                
            }
            //   - *Check if any processes need to move from NotStarted to Ready (based on elapsed time), and if so put that process in the ready queue
            //std::cout << current_time - start << "\n";
            //std::cout << processes.at(i)->getStartTime() << "\n";
            if(processes.at(i)->getState() == Process::State::NotStarted && processes.at(i)->getStartTime() <= current_time - start) {
                //then add process to scheduling queue based on algorithm
                //std::cout << "TRUE\n";
                shared_data->ready_queue.push_back(processes.at(i));
                shared_data->ready_queue.back()->setState(Process::State::Ready, currentTime());
                if(shared_data->algorithm == SJF) {
                    //inserts process at i at and and bubbles it to correct position
                    shared_data->ready_queue.sort(SJFoperate);
                } else if(shared_data->algorithm == PP) {
                    shared_data->ready_queue.sort(SJFoperate);
                }
            }
        }



        

        //   - *Check if any processes have finished their I/O burst, and if so put that process back in the ready queue


        //   - *Check if any running process need to be interrupted (RR time slice expires or newly ready process has higher priority)
        //   - *Sort the ready queue (if needed - based on scheduling algorithm)
        //   - Determine if all processes are in the terminated state
        //   - * = accesses shared data (ready queue), so be sure to use proper synchronization
        
        
        for(int i = 0; i < processes.size(); i++) {
            processes.at(i)->updateProcess(currentTime(), start);
        }

        // output process status table
        num_lines = printProcessOutput(processes, shared_data->mutex);

        // sleep 50 ms
        usleep(50000);

        int check = 0;
        for(int i = 0; i < processes.size(); i++) {
            if(processes.at(i)->getState() == Process::State::Terminated) {
                check++;
            }
        }

        if(check == processes.size()) {
            
            shared_data->mutex.lock();
            shared_data->all_terminated = true;
            shared_data->mutex.unlock();
            //return 0;
        }

        bool toBreak = false;

        //breaks out of loops when finding a higher priority to save runtime
        shared_data->mutex.lock();
        if(shared_data->algorithm == PP) {
            for(int i = 0; i < processes.size(); i++) {
            for(int j = 0; j < processes.size(); j++) {
                //if process j is ready and has higher priority than process i, and process i is running, interrupt process i
                if(processes.at(j)->getState() == Process::State::Ready && processes.at(i)->getState() == Process::State::Running) {
                    if(processes.at(j)->getPriority() < processes.at(i)->getPriority()) {
                        processes.at(i)->interrupt();
                        toBreak = true;
                        break;
                    }
                }
            }
            //can't remember if break only breaks out of one loop, so did this
            if(toBreak == true) {
                break;
            }
        }
        }
        shared_data->mutex.unlock();
        
    }
    
    //std::cout << "TEST";

    // wait for threads to finish
    for (i = 0; i < num_cores; i++)
    {
        
        schedule_threads[i].join();
        //return 0;
    }
    

    // print final statistics
    //  - CPU utilization
    //  - Throughput
    //     - Average for first 50% of processes finished
    //     - Average for second 50% of processes finished
    //     - Overall average
    //  - Average turnaround time
    //  - Average waiting time
    clearOutput(num_lines);
    num_lines = printProcessOutput(processes, shared_data->mutex);

    double cpuTimeSum;
    double turnTimeSum;
    double cpuUtilizationPercent;
    double turnTimeAvg;
    //double turnTimes[processes.size()];
    double throughputFirst;
    double throughputLast;
    double throughputTotal;
    double waitTimeAvg;

    for(int i = 0; i < processes.size(); i++) {
        cpuTimeSum += processes.at(i)->getCpuTime();
        turnTimeSum += processes.at(i)->getTurnaroundTime();

        //turnTimes[i] = processes.at(i)->getTurnaroundTime();
        
    }

    //std::sort(turnTimes, turnTimes + processes.size());

    for(int i = 0; i < processes.size() / 2; i++) {
        throughputFirst += shared_data->turnTimes.at(i);
    }
    throughputFirst = (processes.size() / 2) / throughputFirst;

    for(int i = processes.size() - (processes.size() / 2); i < processes.size(); i++) {
        throughputLast += shared_data->turnTimes.at(i);
    }
    throughputLast = (processes.size() - (processes.size() / 2)) / throughputLast;

    for(int i = 0; i < processes.size(); i++) {
        throughputTotal += shared_data->turnTimes.at(i);
    }
    throughputTotal = processes.size() / throughputTotal;
    
    cpuUtilizationPercent = cpuTimeSum / turnTimeSum * 100.0;
    turnTimeAvg = turnTimeSum / processes.size();

    for(int i = 0; i < processes.size(); i++) {
        waitTimeAvg += processes.at(i)->getWaitTime();
    }
    waitTimeAvg = waitTimeAvg / processes.size();


    //printf("Sum of CPU time: %.1f\n", cpuTimeSum);
    //printf("Sum of turnaround time: %.1f\n", turnTimeSum);
    printf("CPU Utilization: %.2f%%\n", cpuUtilizationPercent);

    printf("Average turnaround time: %.1f\n", turnTimeAvg);
    printf("Average throughput for first 50%%: %f processes per second\n", throughputFirst);
    printf("Average throughput for last 50%%: %f processes per second\n", throughputLast);
    printf("Average throughput: %f processes per second\n", throughputTotal);
    printf("Average wait time: %.1f\n", waitTimeAvg);

    // Clean up before quitting program
    processes.clear();

    return 0;
}

void coreRunProcesses(uint8_t core_id, SchedulerData *shared_data)
{
    SjfComparator SJFoperate;
    PpComparator PPoperate;
    // Work to be done by each core idependent of the other cores
    // Repeat until all processes in terminated state:
    while (!(shared_data->all_terminated)) {
        //   - *Get process at front of ready queue
        Process *thisProcess;

        shared_data->mutex.lock();
        if(!shared_data->ready_queue.empty()) {
            //std::cout << "AAAAA";
            thisProcess = shared_data->ready_queue.front();
            shared_data->ready_queue.pop_front();
        } else {
            shared_data->mutex.unlock();
            continue;
        }
        uint32_t context = shared_data->context_switch;
        shared_data->mutex.unlock();
        
        //if the process hasn't started, do initial stuff
        
        /*if(thisProcess->getCpuTime() == 0) {
            thisProcess->setBurstStartTime(currentTime());
        
        }*/

        thisProcess->setCpuCore(core_id);

        thisProcess->setBurstStartTime(currentTime());
        
        thisProcess->setState(Process::State::Running, currentTime());
    
        //   - Simulate the processes running until one of the following:
        //     - CPU burst time has elapsed
        //     - Interrupted (RR time slice has elapsed or process preempted by higher priority process)
        
        while(currentTime() - thisProcess->getBurstStartTime() < (uint64_t)thisProcess->getBurstTime(thisProcess->getBurstIdx())) {
            //time is elapsing
            //std::cout << shared_data->algorithm <<std::endl;

            if(thisProcess->isInterrupted()) {
                break;
            }
            shared_data->mutex.lock();
            if(currentTime() - thisProcess->getBurstStartTime() >= shared_data->time_slice && shared_data->algorithm == RR && thisProcess->getRemainingTime() > 0) {
                //std::cout << "hello\n" <<std::endl;
                thisProcess->interrupt();
                shared_data->mutex.unlock();
                break;
            }
            shared_data->mutex.unlock();
            
        }
        //std::cout << "test";
        //  - Place the process back in the appropriate queue
        //     - I/O queue if CPU burst finished (and process not finished) -- no actual queue, simply set state to IO
        //     - Terminated if CPU burst finished and no more bursts remain -- no actual queue, simply set state to Terminated
        //     - *Ready queue if interrupted (be sure to modify the CPU burst time to now reflect the remaining time)
        

        if(thisProcess->isInterrupted()) {
            thisProcess->interruptHandled();
            thisProcess->setState(Process::State::Ready, currentTime());
            thisProcess->updateBurstTime(thisProcess->getBurstIdx(), thisProcess->getBurstTime(thisProcess->getBurstIdx()) - (currentTime() - thisProcess->getBurstStartTime()));
            shared_data->mutex.lock();
            shared_data->ready_queue.push_back(thisProcess);
            if(shared_data->algorithm == SJF) {
                shared_data->ready_queue.sort(SJFoperate);
            } else if (shared_data->algorithm == PP) {
                shared_data->ready_queue.sort(PPoperate);
            }
            
            shared_data->mutex.unlock();       
        } else if(currentTime() - thisProcess->getBurstStartTime() >= thisProcess->getBurstTime(thisProcess->getBurstIdx()) && thisProcess->getBurstIdx() == thisProcess->getNumBursts() - 1) {
            thisProcess->setState(Process::State::Terminated, currentTime());
            shared_data->mutex.lock();
            shared_data->turnTimes.push_back(thisProcess->getTurnaroundTime());
            shared_data->mutex.unlock();
        } else if(currentTime() - thisProcess->getBurstStartTime() >= thisProcess->getBurstTime(thisProcess->getBurstIdx())) {
            //if cpu burst finished
            //std::cout << "TEST";
            thisProcess->setState(Process::State::IO, currentTime());
            thisProcess->updateBurstIdx();
            thisProcess->setIOStartTime(currentTime());

            
        } 
        
        thisProcess->setCpuCore(-1);
        //  - Wait context switching time
        
        usleep(context);
        //  - * = accesses shared data (ready queue), so be sure to use proper synchronization
    }
    //std::cout << core_id << "\n\n\n test" << core_id;
    
}

int printProcessOutput(std::vector<Process*>& processes, std::mutex& mutex)
{
    int i;
    int num_lines = 2;
    std::lock_guard<std::mutex> lock(mutex);
    printf("|   PID | Priority |      State | Core | Turn Time | Wait Time | CPU Time | Remain Time |\n");
    printf("+-------+----------+------------+------+-----------+-----------+----------+-------------+\n");
    for (i = 0; i < processes.size(); i++)
    {
        if (processes[i]->getState() != Process::State::NotStarted)
        {
            uint16_t pid = processes[i]->getPid();
            uint8_t priority = processes[i]->getPriority();
            std::string process_state = processStateToString(processes[i]->getState());
            int8_t core = processes[i]->getCpuCore();
            std::string cpu_core = (core >= 0) ? std::to_string(core) : "--";
            double turn_time = processes[i]->getTurnaroundTime();
            double wait_time = processes[i]->getWaitTime();
            double cpu_time = processes[i]->getCpuTime();
            double remain_time = processes[i]->getRemainingTime();
            printf("| %5u | %8u | %10s | %4s | %9.1lf | %9.1lf | %8.1lf | %11.1lf |\n", 
                   pid, priority, process_state.c_str(), cpu_core.c_str(), turn_time, 
                   wait_time, cpu_time, remain_time);
            num_lines++;
        }
    }
    return num_lines;
}

void clearOutput(int num_lines)
{
    int i;
    for (i = 0; i < num_lines; i++)
    {
        fputs("\033[A\033[2K", stdout);
    }
    rewind(stdout);
    fflush(stdout);
}

uint64_t currentTime()
{
    uint64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::system_clock::now().time_since_epoch()).count();
    return ms;
}

std::string processStateToString(Process::State state)
{
    std::string str;
    switch (state)
    {
        case Process::State::NotStarted:
            str = "not started";
            break;
        case Process::State::Ready:
            str = "ready";
            break;
        case Process::State::Running:
            str = "running";
            break;
        case Process::State::IO:
            str = "i/o";
            break;
        case Process::State::Terminated:
            str = "terminated";
            break;
        default:
            str = "unknown";
            break;
    }
    return str;
}
