/**
 *
 * @file interrupts.cpp
 * @author Sasisekhar Govind
 *
 */

#include <interrupts_101304877_101294162.hpp>

std::tuple<std::string, std::string, int> simulate_trace(std::vector<std::string> trace_file, int time, std::vector<std::string> vectors, std::vector<int> delays, std::vector<external_file> external_files, PCB current, std::vector<PCB> wait_queue)
{

    std::string trace;              //!< string to store single line of trace file
    std::string execution = "";     //!< string to accumulate the execution output
    std::string system_status = ""; //!< string to accumulate the system status output
    int current_time = time;

    // parse each line of the input trace file. 'for' loop to keep track of indices.
    for (size_t i = 0; i < trace_file.size(); i++)
    {
        auto trace = trace_file[i];

        auto [activity, duration_intr, program_name] = parse_trace(trace);

        if (activity == "CPU")
        { // As per Assignment 1
            execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", CPU Burst\n";
            current_time += duration_intr;
        }
        else if (activity == "SYSCALL")
        { // As per Assignment 1
            auto [intr, time] = intr_boilerplate(current_time, duration_intr, 10, vectors);
            execution += intr;
            current_time = time;

            execution += std::to_string(current_time) + ", " + std::to_string(delays[duration_intr]) + ", SYSCALL ISR\n";
            current_time += delays[duration_intr];

            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
        }
        else if (activity == "END_IO")
        {
            auto [intr, time] = intr_boilerplate(current_time, duration_intr, 10, vectors);
            current_time = time;
            execution += intr;

            execution += std::to_string(current_time) + ", " + std::to_string(delays[duration_intr]) + ", ENDIO ISR\n";
            current_time += delays[duration_intr];

            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
        }
        else if (activity == "FORK")
        {
            auto [intr, time] = intr_boilerplate(current_time, 2, 10, vectors);
            execution += intr;
            current_time = time;

            ///////////////////////////////////////////////////////////////////////////////////////////
            // FORK: clone PCB, schedule child to run immediately, record system status
            ///////////////////////////////////////////////////////////////////////////////////////////

            // isr: cloning PCB
            execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", cloning the PCB\n";
            current_time += duration_intr;

            // call scheduler
            execution += std::to_string(current_time) + ", 0, scheduler called\n";
            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
            ///////////////////////////////////////////////////////////////////////////////////////////

            // The following loop helps you do 2 things:
            //  * Collect the trace of the chile (and only the child, skip parent)
            //  * Get the index of where the parent is supposed to start executing from
            std::vector<std::string> child_trace; // child executes program1
            bool skip = true;
            bool exec_flag = false;
            int parent_index = 0;

            for (size_t j = i; j < trace_file.size(); j++)
            {
                auto [_activity, _duration, _pn] = parse_trace(trace_file[j]);
                if (skip && _activity == "IF_CHILD")
                {
                    skip = false;
                    continue;
                }
                else if (_activity == "IF_PARENT")
                {
                    skip = true;
                    parent_index = j;
                    if (exec_flag)
                    {
                        break;
                    }
                }
                else if (skip && _activity == "ENDIF")
                {
                    skip = false;
                    continue;
                }
                else if (!skip && _activity == "EXEC")
                {
                    skip = true;
                    child_trace.push_back(trace_file[j]);
                    exec_flag = true;
                }

                if (!skip)
                {
                    child_trace.push_back(trace_file[j]);
                }
            }
            i = parent_index;

            ///////////////////////////////////////////////////////////////////////////////////////////
            // With the child's trace, run the child (HINT: think recursion)
            // Create new child PID
            int max_pid = (int)current.PID;
            for (const auto &p : wait_queue)
            {
                if ((int)p.PID > max_pid)
                    max_pid = (int)p.PID;
            }
            unsigned int child_pid = (unsigned int)(max_pid + 1);

            // Save parent state
            PCB parent_copy = current;

            // create child pcb (copy of paren initially)
            PCB child(child_pid, parent_copy.PID, parent_copy.program_name, parent_copy.size, -1);

            // Try allocate memory for child (will pick smallest fitting partition)
            if (!allocate_memory(&child))
            {
                // allocation failed -> child stays without partition (-1)
                execution += std::to_string(current_time) + ", 0, memory allocation failed for child PID " + std::to_string(child.PID) + "\n";
            }

            // Put parent in wait queue and set child as current running process
            wait_queue.push_back(parent_copy);
            current = child;

            // system status after fork
            system_status += "time: " + std::to_string(current_time) + "; current trace: " + trace + "\n";
            system_status += print_PCB(current, wait_queue);

            // recursively run child trace
            if (!child_trace.empty())
            {
                auto [child_exec, child_status, child_time] = simulate_trace(child_trace, current_time, vectors, delays, external_files, current, wait_queue);
                execution += child_exec;
                system_status += child_status;
                current_time = child_time;
            }

            // Child finished: restore parent as current and remove parent from wait_queue
            current = parent_copy;
            for (auto it = wait_queue.begin(); it != wait_queue.end(); ++it)
            {
                if (it->PID == parent_copy.PID)
                {
                    wait_queue.erase(it);
                    break;
                }
            }

            ///////////////////////////////////////////////////////////////////////////////////////////
        }
        else if (activity == "EXEC")
        {
            auto [intr, time] = intr_boilerplate(current_time, 3, 10, vectors);
            current_time = time;
            execution += intr;

            ///////////////////////////////////////////////////////////////////////////////////////////
            // EXEC: load new program into memory, update PCB, scheduler, then run the new program
            ///////////////////////////////////////////////////////////////////////////////////////////

            // Find program size from external_files
            unsigned int prog_size = get_size(program_name, external_files);
            if (prog_size == (unsigned int)-1)
            {
                // Program not found; log and skip
                execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", Program " + program_name + " not found\n";
                current_time += duration_intr;
                execution += std::to_string(current_time) + ", 1, IRET\n";
                current_time += 1;
                break;
            }

            // Log program size (use duration_intr for ISR step duration)
            execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", Program is " + std::to_string(prog_size) + " Mb large\n";
            current_time += duration_intr;

            // Before allocating new partition, free current partition (if any)
            if (current.partition_number != -1)
            {
                free_memory(&current);
            }

            // Simulate loader: 15 ms per Mb
            int loader_time = (int)prog_size * 15;

            // Update current PCB with new program name and size and allocate memory
            current.program_name = program_name;
            current.size = prog_size;

            // check if program can even be loaded before commiting time to load
            if (!allocate_memory(&current))
            {
                execution += std::to_string(current_time) + ", 0, memory allocation failed for EXEC (PID " + std::to_string(current.PID) + ")\n";
                break;
            }

            execution += std::to_string(current_time) + ", " + std::to_string(loader_time) + ", loading program into memory\n";
            current_time += loader_time;

            // Marking partition and updating PCB (use small fixed times to match samples)
            execution += std::to_string(current_time) + ", 3, marking partition as occupied\n";
            current_time += 3;

            execution += std::to_string(current_time) + ", 6, updating PCB\n";
            current_time += 6;

            // scheduler (0 time) and IRET
            execution += std::to_string(current_time) + ", 0, scheduler called\n";
            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;

            // Snapshot system status right after EXEC before running external trace
            system_status += "time: " + std::to_string(current_time) + "; current trace: " + trace + "\n";
            system_status += "c copied the new process onto init. Notice how the partition was reassigned\n"; // explanatory line requested
            system_status += print_PCB(current, wait_queue);

            // Run the new program's trace immediately (simulate executing the loaded program)
            {
                std::ifstream exec_trace_file(program_name + ".txt");
                if (exec_trace_file)
                {
                    std::vector<std::string> exec_traces;
                    std::string line;
                    while (std::getline(exec_trace_file, line))
                        exec_traces.push_back(line);

                    if (!exec_traces.empty())
                    {
                        auto [child_exec, child_status, child_time] = simulate_trace(exec_traces, current_time, vectors, delays, external_files, current, wait_queue);
                        execution += child_exec;
                        system_status += child_status;
                        current_time = child_time;
                    }
                }
                else
                {
                    execution += std::to_string(current_time) + ", 0, external program trace " + program_name + ".txt not found\n";
                }
            }

            std::ifstream exec_trace_file(program_name + ".txt");

            std::vector<std::string> exec_traces;
            std::string exec_trace;
            while (std::getline(exec_trace_file, exec_trace))
            {
                exec_traces.push_back(exec_trace);
            }
            break; // Why is this important? (answer in report)
        }
    }

    return {execution, system_status, current_time};
}

int main(int argc, char **argv)
{

    // vectors is a C++ std::vector of strings that contain the address of the ISR
    // delays  is a C++ std::vector of ints that contain the delays of each device
    // the index of these elemens is the device number, starting from 0
    // external_files is a C++ std::vector of the struct 'external_file'. Check the struct in
    // interrupt.hpp to know more.
    auto [vectors, delays, external_files] = parse_args(argc, argv);
    std::ifstream input_file(argv[1]);

    // Just a sanity check to know what files you have
    print_external_files(external_files);

    // Make initial PCB (notice how partition is not assigned yet)
    PCB current(0, -1, "init", 1, -1);
    // Update memory (partition is assigned here, you must implement this function)
    if (!allocate_memory(&current))
    {
        std::cerr << "ERROR! Memory allocation failed!" << std::endl;
    }

    std::vector<PCB> wait_queue;

    /******************ADD YOUR VARIABLES HERE*************************/

    /******************************************************************/

    // Converting the trace file into a vector of strings.
    std::vector<std::string> trace_file;
    std::string trace;
    while (std::getline(input_file, trace))
    {
        trace_file.push_back(trace);
    }

    auto [execution, system_status, _] = simulate_trace(trace_file,
                                                        0,
                                                        vectors,
                                                        delays,
                                                        external_files,
                                                        current,
                                                        wait_queue);

    input_file.close();

    write_output(execution, "execution.txt");
    write_output(system_status, "system_status.txt");

    return 0;
}
