// CSE 375/475 Assignment #1
// Fall 2021
//
// Description: This file implements a function 'run_custom_tests' that should be able to use
// the configuration information to drive tests that evaluate the correctness
// and performance of the map_t object.

#include <iostream>
#include <ctime>
#include <time.h>
#include <map>
#include <stdlib.h>
#include "config_t.h"
#include "tests.h"
#include <thread>
#include <future>
#include <unistd.h>
#include <mutex>
#include <chrono>

#include "simplemap.h"

// Step 1
    // Define a simplemap_t of types <int,float>
    // this map represents a collection of bank accounts:
    // each account has a unique ID of type int;
    // each account has an amount of fund of type float.
simplemap_t<int, float> accounts = simplemap_t<int, float>();

std::mutex myMutex1, myMutex2, myMutex3;

void printer(int k, float v) {
	std::cout<<"<"<<k<<","<<v<<">"<< std::endl;
}

// Step 3
// Define a function "deposit" that selects two random bank accounts
// and an amount. This amount is subtracted from the amount
// of the first account and summed to the amount of the second
// account. In practice, give two accounts B1 and B2, and a value V,
// the function performs B1-=V and B2+=V.
// The execution of the whole function should happen atomically:
// no operation should happen on B1 and B2 (or on the whole map?)
// while the function executes.
void deposit(simplemap_t<int, float> &accounts, int key_max){
	int B1, B2;
    int size = key_max;
    do {
        B1 = rand() % size;
        B2 = rand() % size;
    } while(B1 == B2);

    int rand_amount = rand() % 100 + 1;
    myMutex1.lock();

    float B1_amount = accounts.lookup(B1).first;
    float B2_amount =  accounts.lookup(B2).first;
    B1_amount -= rand_amount;
    B2_amount += rand_amount;
    //std::this_thread::sleep_for(std::chrono::milliseconds(10));
    myMutex2.lock();
    accounts.update(B1, B1_amount);
    myMutex1.unlock();
    accounts.update(B2, B2_amount);
    //accounts.apply(printer);
    myMutex2.unlock();
}

// Step 4
// Define a function "balance" that sums the amount of all the
// bank accounts in the map. In order to have a consistent result,
// the execution of this function should happen atomically:
// no other deposit operations should interleave.
float balance(simplemap_t<int, float> accounts, int key_max){
    float sum = 0;
    std::lock_guard<std::mutex> guard(myMutex3);
    for (int i = 0; i < key_max; ++i){
        std::pair<float, bool> value_pair = accounts.lookup(i);
        sum += value_pair.first;
    }
    return sum;
}

// Step 5
// Define a function 'do_work', which has a for-loop that
// iterates for config_t.iters times. In each iteration,
// the function 'deposit' should be called with 95% of the probability;
// otherwise (the rest 5%) the function 'balance' should be called.
// The function 'do_work' should measure 'exec_time_i', which is the
// time needed to perform the entire for-loop. This time will be shared with
// the main thread once the thread executing the 'do_work' joins its execution
// with the main thread.
void do_work(simplemap_t<int, float> &accounts, int key_max, int iters, std::promise<double> * prom_exec_time){
    clock_t start = clock();
    int rand_num;
    for (int i = 0; i < iters; ++i){
        rand_num = rand() % 100 + 1;
        if (rand_num <= 95) {
            deposit(accounts, key_max);
        } else {
            //std::cout << "5% do: " << balance(accounts, key_max) << std::endl;
            balance(accounts, key_max);
        }
    }
    clock_t end = clock();
    double exec_time_i = ((double) (end - start)) / CLOCKS_PER_SEC;
    prom_exec_time->set_value(exec_time_i);
    printf("for loop took %f seconds to execute\n", exec_time_i);
}

  /*void do_work(simplemap_t<int, float> &accounts, int key_max, int iters){
    clock_t start = clock();
    for (int i = 0; i < iters; ++i){
        int rand_num = rand() % 100 + 1;
        if (rand_num <= 95) {
            deposit(accounts, key_max);
        } else {
    //std::cout << "5% do: " << balance(accounts, key_max) << std::endl;
            balance(accounts, key_max);
        }
    }
    clock_t end = clock();
    double exec_time_i = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("for loop took %f seconds to execute\n", exec_time_i);
    }*/

void run_custom_tests(config_t& cfg) {
    // Step 2
    // Populate the entire map with the 'insert' function
    // Initialize the map in a way the sum of the amounts of
    // all the accounts in the map is 100000
    float  amount = 100000.0 / cfg.key_max;
    for (int i = 0; i < cfg.key_max; ++i){
        if (!accounts.insert(i, amount)) {
            std::cout << "Key has exists, initialize failed" << std::endl;
            break;
        }
    }
    //accounts.apply(printer);
    
    // Step 6
    // The evaluation should be performed in the following way:
    // - the main thread creates #threads threads (as defined in config_t)
    //   << use std:threds >>
    // - each thread executes the function 'do_work' until completion
    // - the (main) spawning thread waits for all the threads to be executed
    //   << use std::thread::join() >>
    //	 and collect all the 'exec_time_i' from each joining thread
    //   << consider using std::future for retireving 'exec_time_i' after the thread finishes its do_work>>
    // - once all the threads have joined, the function "balance" must be called

    // Make sure evey invocation of the "balance" function returns 100000.

    double total_exec_time = 0.0;
    std::thread threads[cfg.threads];
    for (int t = 0; t < cfg.threads; ++t){
        std::promise<double> prom_exec_time;
        std::future<double> future_exec_time = prom_exec_time.get_future();
        threads[t] = std:: thread(&do_work, std::ref(accounts), std::ref(cfg.key_max), std::ref(cfg.iters), &prom_exec_time);
        total_exec_time += future_exec_time.get();
    }

    for (auto& th : threads) th.join();
    std::cout << "the end: "<< balance(accounts, cfg.key_max) << std::endl;
    std::cout << "collect all the execution time: " << total_exec_time << std::endl;

    // Step 7
    // Now configure your application to perform the SAME TOTAL amount
    // of iterations just executed, but all done by a single thread.
    // Measure the time to perform them and compare with the time
    // previously collected.
    // Which conclusion can you draw?
    // Which optimization can you do to the single-threaded execution in
    // order to improve its performance?

    //do_work(accounts, cfg.key_max, cfg.iters);
    //std::cout << "the end: "<< balance(accounts, cfg.key_max) << std::endl;

    // Step 8
    // Remove all the items in the map by leveraging the 'remove' function of the map
    // Destroy all the allocated resources (if any)
    // Execution terminates.
    // If you reach this stage happy, then you did a good job!

    for (int i = 0; i < cfg.key_max; ++i){
        accounts.remove(i);
    }
    accounts.apply(printer);
    // Final step: Produce plot
// I expect each submission to include at least one plot in which
// the x-axis is the concurrent threads used {1;2;4;8}
// the y-axis is the application execution time.
// The performance at 1 thread must be the sequential
// application without synchronization primitives

// You might need the following function to print the entire map.
// Attention if you use it while multiple threads are operating
//accounts.apply(printer);

}

void test_driver(config_t &cfg) {
	run_custom_tests(cfg);
}
