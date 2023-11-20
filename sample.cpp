#include <iostream>

#include "Solver.h"
#include "Graph.h"
#include "clausebuf.h"
#include "ts.h"

extern "C" {
#include "aiger.h"
}

void printHelpMessage (const char * argv0) {
    std::cout << "Usage: " << argv0 << "[options] aiger\n" ;
    std::cout << "Options: \n" ;
    std::cout << "          -f inv.cnf : load frame for cex sampling\n" ;
    std::cout << "          -g graph   : dump graph to graph \n" ;
    std::cout << "          -i in.cnf out.cnf: load frame for filtering \n" ;
    std::cout << "          -h : print help message \n" ;
}

int main(int argc, char ** argv) {

  const char * fname = NULL;
  const char * framebuf = NULL;
  const char * dump_graph = NULL;
  const char * outframebuf = NULL;
  bool sampling_cex = false;
  bool filtering_frame = false;

  if (argc > 1) {
    int idx = 1;
    for (; idx < argc - 1 ; ++ idx) {
      if(argv[idx] == std::string("-f")) {
        framebuf = argv[++idx];
        sampling_cex = true;
      } else if (argv[idx] == std::string("-g"))
        dump_graph = argv[++idx];
      else if (argv[idx] == std::string("-i")) {
        framebuf = argv[++idx];
        outframebuf = argv[++idx];
        filtering_frame = true;
      }
      else if (argv[idx] == std::string("-h")) {
        printHelpMessage(argv[0]);
        return 0;
      }
      else {
        std::cout << "unknown option:" << argv[idx] << std::endl;
        printHelpMessage(argv[0]);
        return 1;
      }
    }
    if (idx != argc - 1) {
      printHelpMessage(argv[0]);
      return 1;
    }
    fname = argv[argc-1];
  } else {
    printHelpMessage(argv[0]);
    return 1;
  }

  // read AIGER model
  aiger * aig = aiger_init();
  const char * msg;
  msg = aiger_open_and_read_from_file(aig, fname);

  if (msg) {
    std::cout << msg << std::endl;
    return 1;
  }

  if (dump_graph) {
    Graph model (aig);
    model.dump(dump_graph);
  }
  // construct AIG -> graph
  // sample model: P /\ T /\ neg P'
  // do : generalize predecessor
  // load inv.cnf and find it?

  // construct transition relation
  if(filtering_frame) {
    ClauseBuf buf, bufout;
    buf.from_file(framebuf);

    TransitionSystem ts(aig, 0);

    std::cout << "Loaded " << buf.clauses.size() << " clauses from " << framebuf << std::endl;
    std::cout << "Loaded TS (SV: " << ts.statevars().size() << " IV:" << ts.inputvars().size() << " ) from "  << fname << std::endl;
    if(is_ts_trivially_unsafe(ts)) {
      std::cout << "TS is trivially unsafe! Will not filter clauses." << std::endl;
      return 1;
    }

    filter_clauses(buf, bufout, ts);
    bufout.dump(outframebuf);

    std::cout << "Write " << bufout.clauses.size() << " clauses to " << outframebuf << std::endl;
    aiger_reset(aig);
    return 0;
  }

  if(sampling_cex) {
    //  ClauseBuf buf;
    //buf.from_file(framebuf);
    // then sample for N models with clausebuf
    // } else {
    // random sampling
    TransitionSystem ts(aig, 0);
    std::vector<ctiModel> m;
    auto ret = sample_cti(ts, 100, m);
    std::cout << "Total samples: "<< ret << std::endl;
    // sample model: P /\ T /\ neg P'
  }
  
  aiger_reset(aig);
  return 0;
}

