/*****************************************************************************************[Main.cc]
Copyright (c) 2003-2006, Niklas Een, Niklas Sorensson
Copyright (c) 2007,      Niklas Sorensson

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#include <errno.h>

#include <signal.h>
#include <zlib.h>
#include <sys/resource.h>
#include <mpi.h>
#include "../utils/System.h"
#include "../utils/ParseUtils.h"
#include "../utils/Options.h"
#include "../core/Dimacs.h"
#include "../simp/SimpSolver.h"
#include <boost/coroutine2/all.hpp>
#include <boost/bind.hpp>
#include <iostream>
#include <vector>

using namespace Minisat;

//=================================================================================================

void printStats(Solver& solver)
{
    double cpu_time = cpuTime();
//    double mem_used = memUsedPeak();
//    printf("restarts              : %" PRIu64"\n", solver.starts);
//    printf("conflicts             : %-12" PRIu64"   (%.0f /sec)\n", solver.conflicts   , solver.conflicts   /cpu_time);
//    printf("decisions             : %-12" PRIu64"   (%4.2f %% random) (%.0f /sec)\n", solver.decisions, (float)solver.rnd_decisions*100 / (float)solver.decisions, solver.decisions   /cpu_time);
//    printf("propagations          : %-12" PRIu64"   (%.0f /sec)\n", solver.propagations, solver.propagations/cpu_time);
//    printf("conflict literals     : %-12" PRIu64"   (%4.2f %% deleted)\n", solver.tot_literals, (solver.max_literals - solver.tot_literals)*100 / (double)solver.max_literals);
//    long double total_actual_rewards = 0;
//    long double total_actual_count = 0;
//    for (int i = 0; i < solver.nVars(); i++) {
//        total_actual_rewards += solver.total_actual_rewards[i];
//        total_actual_count += solver.total_actual_count[i];
//    }
//    printf("actual reward         : %Lf\n", total_actual_rewards / total_actual_count);
//    if (mem_used != 0) printf("Memory used           : %.2f MB\n", mem_used);
//    printf("CPU time              : %g s\n", cpu_time);

//modified by @lavleshm
    printf("CPU time: %g s ", cpu_time);

}


//static Solver* solver;
//static Solver* solver1;
static std::vector<Solver *> solver_ptrs;
// Terminate by notifying the solver and back out gracefully. This is mainly to have a test-case
// for this feature of the Solver as it may take longer than an immediate call to '_exit()'.
static void SIGINT_interrupt(int signum) {
//    solver->interrupt();
//    solver1->interrupt();
    for(Solver* s : solver_ptrs) s->interrupt();
}

// Note that '_exit()' rather than 'exit()' has to be used. The reason is that 'exit()' calls
// destructors and may cause deadlocks if a malloc/free function happens to be running (these
// functions are guarded by locks for multithreaded use).
static void SIGINT_exit(int signum) {
    printf("\n"); printf("*** INTERRUPTED ***\n");
    if (solver_ptrs[0]->verbosity/*solver->verbosity*/ > 0){
        printStats(/*solver*/*solver_ptrs[0]);
//        printStats(*solver1);
        printf("\n"); printf("*** INTERRUPTED ***\n"); }
    _exit(1); }

//struct tester_arg{
//    SimpSolver *S;
//    vec<Lit>* dummy;
//};
//
//void tester(void* arg){
//    struct tester_arg * aarg = (struct tester_arg *)arg;
//    aarg->S->solveLimited(*aarg->dummy);
//}
//void create_test_tasks(SimpSolver* simpSolver, vec<Lit>* dummy){
//    struct tester_arg * ta = (tester_arg *)malloc(sizeof(tester_arg));
//    ta->S = simpSolver;
//    ta->dummy = dummy;
//    scheduler_create_task(tester, ta);
//}

//=================================================================================================
// Main:

int main(int argc, char** argv)
{
    try {
        setUsageHelp("USAGE: %s [options] <input-file> <result-output-file>\n\n  where input may be either in plain or gzipped DIMACS.\n");
        // printf("This is MiniSat 2.0 beta\n");
        
#if defined(__linux__)
        fpu_control_t oldcw, newcw;
        _FPU_GETCW(oldcw); newcw = (oldcw & ~_FPU_EXTENDED) | _FPU_DOUBLE; _FPU_SETCW(newcw);
//        printf("WARNING: for repeatability, setting FPU to use double precision\n");  //modified by @lavleshm
#endif
        // Extra options:
        //
        IntOption    verb   ("MAIN", "verb",   "Verbosity level (0=silent, 1=some, 2=more).", 1, IntRange(0, 2));
        BoolOption   pre    ("MAIN", "pre",    "Completely turn on/off any preprocessing.", true);
        StringOption dimacs ("MAIN", "dimacs", "If given, stop after preprocessing and write the result to this file.");
        StringOption assumptions ("MAIN", "assumptions", "If given, use the assumptions in the file.");
        IntOption    cpu_lim("MAIN", "cpu-lim","Limit on CPU time allowed in seconds.\n", INT32_MAX, IntRange(0, INT32_MAX));
        IntOption    mem_lim("MAIN", "mem-lim","Limit on memory usage in megabytes.\n", INT32_MAX, IntRange(0, INT32_MAX));
        // adding options to take number of solvers instances
        IntOption num_solvers    ("MAIN", "solvers", "Number of solver instances to execute in interleaved manner.\n", 1, IntRange(1, 4));

        parseOptions(argc, argv, true);
        
//        std::vector<SimpSolver> solvers((int) num_solvers);

        SimpSolver solvers[(int) num_solvers];
        int num_instances = (int)num_solvers;
        for(int i = 0 ; i < num_instances ; i ++){
            solver_ptrs.push_back(&solvers[i]);
        }
//        SimpSolver S0/*, S1*/;
        int count = 0;
        for(SimpSolver& S : solvers){
                S.Mpi_rank = count++;
                S.random_seed = S.Mpi_rank * S.random_seed + 334454;
                S.iterations = 0;
        }
//        S0.Mpi_rank = 0;
//        S1.Mpi_rank = 1;
        /* Initializing MPI and updating solver state -----------------------------*/

//        MPI_Init( &argc, &argv );
//        MPI_Comm_size(MPI_COMM_WORLD, &S.Comm_size);
//        MPI_Comm_rank(MPI_COMM_WORLD, &S.Mpi_rank);
//        S0.random_seed = S0.Mpi_rank*S0.random_seed + 37891; //constant addition is used to get initial seed as non zero
//        S1.random_seed = S1.Mpi_rank*S1.random_seed + 67867;
//        S0.rnd_init_act = true;
//        std::cout<<"[S0 seed]: "<<S0.random_seed<<"[S1 seed]: "<<S1.random_seed<<std::endl;
//        MPI_Pcontrol(1);
//        MPI_Pcontrol(2);
//        S.rnd_init_act = true;
//        S.lc_file = "file_" + std::to_string(S.Mpi_rank);
//        std::ofstream myfile;
//        myfile.open(S.lc_file);
//        myfile.close();
        //
//        S.rnd_pol = true;
        /*-------------------------------------------------------------------------*/

        double      initial_time = cpuTime();

        if (!pre) {
//            S0.eliminate(true);
//            S1.eliminate(true);
            for(SimpSolver& S :solvers) S.eliminate(true);
        }

//        S0.verbosity = verb;
//        S1.verbosity = verb;
//        solver_ptrs.push_back(&S0);
//        solver = &S0;  //handled up there in first loop
//        solver1 = &S1; //handled up there in first loop
        for(SimpSolver& S :solvers){
            S.verbosity = verb;
        }
        // Use signal handlers that forcibly quit until the solver will be able to respond to
        // interrupts:
        signal(SIGINT, SIGINT_exit);
        signal(SIGXCPU,SIGINT_exit);

        // Set limit on CPU-time:
        if (cpu_lim != INT32_MAX){
            rlimit rl;
            getrlimit(RLIMIT_CPU, &rl);
            if (rl.rlim_max == RLIM_INFINITY || (rlim_t)cpu_lim < rl.rlim_max){
                rl.rlim_cur = cpu_lim;
                if (setrlimit(RLIMIT_CPU, &rl) == -1)
                    printf("WARNING! Could not set resource limit: CPU-time.\n");
            } }

        // Set limit on virtual memory:
        if (mem_lim != INT32_MAX){
            rlim_t new_mem_lim = (rlim_t)mem_lim * 1024*1024;
            rlimit rl;
            getrlimit(RLIMIT_AS, &rl);
            if (rl.rlim_max == RLIM_INFINITY || new_mem_lim < rl.rlim_max){
                rl.rlim_cur = new_mem_lim;
                if (setrlimit(RLIMIT_AS, &rl) == -1)
                    printf("WARNING! Could not set resource limit: Virtual memory.\n");
            } }
        
        if (argc == 1)
            printf("Reading from standard input... Use '--help' for help.\n");

//        gzFile in = (argc == 1) ? gzdopen(0, "rb") : gzopen(argv[1], "rb");
//        gzFile in1 = (argc == 1) ? gzdopen(0, "rb") : gzopen(argv[1], "rb");
        gzFile in [num_instances];
        for(int i = 0 ; i < num_instances ; i++) in[i] = (argc == 1) ? gzdopen(0, "rb") : gzopen(argv[1], "rb");

        if (in == NULL)
            printf("ERROR! Could not open file: %s\n", argc == 1 ? "<stdin>" : argv[1]), exit(1);

        if (solvers[0].verbosity > 0){
            printf("============================[ Problem Statistics ]=============================\n");
            printf("|                                                                             |\n"); }

//        parse_DIMACS(in, S0);
//        parse_DIMACS(in1, S1);
//        gzclose(in);
//        gzclose(in1);
        for(int i = 0 ; i < num_instances ; i++){
            parse_DIMACS(in[i], solvers[i]);
            gzclose(in[i]);
        }

        FILE* res = (argc >= 3) ? fopen(argv[2], "wb") : NULL;

        if (solvers[0].verbosity > 0){
            printf("|  Number of variables:  %12d                                         |\n", solvers[0].nVars());
            printf("|  Number of clauses:    %12d                                         |\n", solvers[0].nClauses()); }

        double parsed_time = cpuTime();
        if (solvers[0].verbosity > 0)
            printf("|  Parse time:           %12.2f s                                       |\n", parsed_time - initial_time);

        // Change to signal-handlers that will only notify the solver and allow it to terminate
        // voluntarily:
        signal(SIGINT, SIGINT_interrupt);
        signal(SIGXCPU,SIGINT_interrupt);

//        S0.eliminate(true); //handled up
//        S1.eliminate(true); //handled up
        double simplified_time = cpuTime();
        if (solvers[0].verbosity > 0){
            printf("|  Simplification time:  %12.2f s                                       |\n", simplified_time - parsed_time);
            printf("|                                                                             |\n"); }
//        if (!S0.okay()){
//            if (res != NULL) fprintf(res, "UNSAT\n"), fclose(res);
//            if (S0.verbosity > 0){
//                printf("===============================================================================\n");
//                printf("Solved by simplification\n");
//                printStats(S0);
//                printf("\n"); }
//            printf("UNSATISFIABLE\n");
//            exit(20);
//        }
        for(SimpSolver& S : solvers){
            if (!S.okay()){
                if (res != NULL) fprintf(res, "UNSAT\n"), fclose(res);
                if (S.verbosity > 0){
                    printf("===============================================================================\n");
                    printf("Solved by simplification\n");
                    printStats(S);
                    printf("\n"); }
                printf("UNSATISFIABLE\n");
                exit(20);
            }
        }
        if (dimacs){
            if (solvers[0].verbosity > 0)
                printf("==============================[ Writing DIMACS ]===============================\n");
            solvers[0].toDimacs((const char*)dimacs);
            if (solvers[0].verbosity > 0)
                printStats(solvers[0]);
//                printStats(S1);
            exit(0);
        }
        vec<Lit> dummy;
        if (assumptions) {
            const char* file_name = assumptions;
            FILE* assertion_file = fopen (file_name, "r");
            if (assertion_file == NULL)
                printf("ERROR! Could not open file: %s\n", file_name), exit(1);
            int i = 0;
            while (fscanf(assertion_file, "%d", &i) == 1) {
                Var v = abs(i) - 1;
                Lit l = i > 0 ? mkLit(v) : ~mkLit(v);
                dummy.push(l);
            }
            fclose(assertion_file);
        }
        for( int i = 0; i < dummy.size(); i++) {
            printf("%s%d\n", sign(dummy[i]) ? "-" : "", var(dummy[i]));
        }
        std::vector<boost::coroutines2::coroutine<void>::push_type> sinks;
        for(int i = 0 ; i < num_instances ; i++){
            using boost::placeholders::_1; //we may need to take it out of the loop
            sinks.emplace_back(boost::bind(&SimpSolver::solveLimited, &solvers[i],_1));
        }
//        using boost::placeholders::_1;
//        boost::coroutines2::coroutine<void>::push_type solver{boost::bind(&SimpSolver::solveLimited, &S0,_1)};
//        boost::coroutines2::coroutine<void>::push_type solver1{boost::bind(&SimpSolver::solveLimited, &S1, _1)};

//        lbool ret = S0.solveLimited(dummy);
//        lbool ret1 =S1.solveLimited(dummy);
        lbool ret;
        bool flag = true; // f1=true, f2=true;
        bool isRunning[sinks.size()];
        for (int j = 0; j < sinks.size(); ++j) {
            isRunning[j] = true;
        }
        while(flag){
            flag = false;

            for (int i = 0; i < sinks.size(); ++i) {
                if(isRunning[i] && sinks[i]()){
                    flag = true;
                    if(solvers[i].readyToShare()){
//                        S0.shareTo(S1);
                        for (int j = 0; j < num_instances; j++){
                            if(j != i) solvers[i].shareTo(solvers[j]);
                        }
                        solvers[i].sharedClauseOut.clear(); //must be called only if clause has been shared with all others
                    }
//                std::cout << "[main]: ....... " << std::endl; //flush here
                }else if(isRunning[i]) {
                    isRunning[i] = false;
                    ret = solvers[i].ret_solveLimited_val;
                    printf("%s ",argv[1]);
                    printStats(solvers[i]);
                    printf("[Rank]: %d [Iterations]: %lld ",solvers[i].Mpi_rank, solvers[i].iterations);
                    printf(ret == l_True ? "SATISFIABLE\n" : ret == l_False ? "UNSATISFIABLE\n" : "INDETERMINATE\n");
                }
            }

//            if(f1 && solver()){
//                flag = true;
//                if(S0.readyToShare()){
//                    S0.shareTo(S1);
//                    S0.sharedClauseOut.clear(); //must be called only if clause has been shared with all others
//                }
////                std::cout << "[main]: ....... " << std::endl; //flush here
//                }else if(isRunning[i]) {
//                    isRunning[i] = false;
//                    ret = solvers[i].ret_solveLimited_val;
//                    printf("%s ",argv[1]);
//                    printStats(solvers[i]);
//                    printf("[Rank]: %d [Iterations]: %lld ",solvers[i].Mpi_rank, solvers[i].iterations);
//                    printf(ret == l_True ? "SATISFIABLE\n" : ret == l_False ? "UNSATISFIABLE\n" : "INDETERMINATE\n");
//                }
//            }

//            if(f1 && solver()){
//                flag = true;
////                if(S0.readyToShare()){
////                    S0.shareTo(S1);
////                    S0.sharedClauseOut.clear(); //must be called only if clause has been shared with all others
////                }
////                std::cout << "[main]: ....... " << std::endl; //flush here
//            }else if(f1) {
//                f1 = false;
//                ret = S0.ret_solveLimited_val;
//                printf("%s ",argv[1]);
//                printStats(S0);
//                printf("[Rank]: %d [Iterations]: %lld ",S0.Mpi_rank, S0.iterations);
//                printf(ret == l_True ? "SATISFIABLE\n" : ret == l_False ? "UNSATISFIABLE\n" : "INDETERMINATE\n");
//            }
//
//            if(f2 && solver1()){
//                flag = true;
//                if(S1.readyToShare()){
//                    S1.shareTo(S0);
//                    S1.sharedClauseOut.clear();
//                }
////                std::cout << "[main]: ....... " << std::endl; //flush here
//            }else if(f2){
//                f2 = false;
//                ret = S1.ret_solveLimited_val;
//                printf("%s ",argv[1]);
//                printStats(S1);
//                printf(ret == l_True ? "SATISFIABLE\n" : ret == l_False ? "UNSATISFIABLE\n" : "INDETERMINATE\n");
//            }

        }


//        if (S0.verbosity > 0){
//            printStats(S0);
//            printStats(S1);
//            printf("\n"); }

        //modified by @lavleshm
//        printf("%s ",argv[1]);
//        if(!f1) {
//            printStats(S0);
//            f1 =true;
//        }
//        if(!f2) {
//            printStats(S1); //also been modified
//            f2 = true;
//        }
        // ...............................................

//        if (S.verbosity > 0){
//            printStats(S);
//            printf("\n");
//        }
//        printf(ret == l_True ? "SATISFIABLE\n" : ret == l_False ? "UNSATISFIABLE\n" : "INDETERMINATE\n");

        //modified by @lavleshm

//        printf(ret == l_True ? "SATISFIABLE\n" : ret == l_False ? "UNSATISFIABLE\n" : "INDETERMINATE\n");
/*Removing temporarily*/
//        if (res != NULL){
//            if (ret == l_True){
//                fprintf(res, "SAT\n");
//                for (int i = 0; i < solvers[0].nVars(); i++)
//                    if (solvers[0].model[i] != l_Undef)
//                        fprintf(res, "%s%s%d", (i==0)?"":" ", (solvers[0].model[i]==l_True)?"":"-", i+1);
//                fprintf(res, " 0\n");
//            }else if (ret == l_False) {
//                fprintf(res, "UNSAT\n");
//                for (int i = 0; i < solvers[0].conflict.size(); i++) {
//                    // Reverse the signs to keep the same sign as the assertion file.
//                    fprintf(res, "%s%d\n", sign(solvers[0].conflict[i]) ? "" : "-", var(solvers[0].conflict[i]) + 1);
//                }
//            } else
//                fprintf(res, "INDET\n");
//            fclose(res);
//        }
/*Removing temporarily*/
        //---------------------------------------------------------------------------------------

//        MPI_Abort(MPI_COMM_WORLD, 0);
//        MPI_Finalize();
//        exit(0);

        //---------------------------------------------------------------------------------------

#ifdef NDEBUG
        exit(ret == l_True ? 10 : ret == l_False ? 20 : 0);     // (faster than "return", which will invoke the destructor for 'Solver')
#else
        return (ret == l_True ? 10 : ret == l_False ? 20 : 0);
#endif
    } catch (OutOfMemoryException&){
        printf("===============================================================================\n");
        printf("INDETERMINATE\n");
        exit(0);
    }
}
