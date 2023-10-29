/*********************************************************************
Copyright (c) 2013, Hongce Zhang

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*********************************************************************/

#include "clausebuf.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

using namespace std;

bool ClauseBuf::from_file(const char *fname) {
    ifstream fin(fname);
    if (!fin.is_open())
        return false;
    int n_clause = 0;
    {
        char header[128];
        fin.getline(header, 128);
        stringstream buf(header);
        string tmp_unsat;
        buf>>tmp_unsat;
        buf>>n_clause;
    }
    if (n_clause <= 0)
        return false;
    
    char buffer[512];
    for (int idx = 0; idx < n_clause; ++ idx) {
        clauses.push_back(std::vector<int>());
        fin.getline(buffer, 512);
        stringstream str(buffer);
        int literal;
        while(str >> literal) {
            clauses.back().push_back(literal);
        }
        if (clauses.back().empty())
            return false;
    }
    return true;
}


void ClauseBuf::dump() const {
    for (const auto & cls : clauses) {
        for (int lit : cls)
            cout << lit << " ";
        cout << endl;
    }
}

bool OrderBuf::from_file(const char *fname) {
    ifstream fin(fname);
    if (!fin.is_open())
        return false;
    int n_size = 0;
    unsigned lit,cnt;
    fin>>n_size;
    for (int idx = 0; idx < n_size; ++ idx) {
        fin >> lit >> cnt;
        pre_est_var_order[(lit >> 1)] = cnt;
    }
    return true;
}
