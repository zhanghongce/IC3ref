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


#ifndef CLAUSEBUF_H_INCLUDED
#define CLAUSEBUF_H_INCLUDED

#include <vector>
#include <unordered_map>
#include <iostream>

struct ClauseBuf{
    std::vector<std::vector<int> > clauses;
    bool from_file(const char *fname);
    void dump() const;
};

struct OrderBuf{
    // let's store a map : var to its occurrance in the file 
    // convert it from literal to variable
    std::unordered_map<unsigned, unsigned> pre_est_var_order; // order pre-estimated
    bool from_file(const char *fname);
    unsigned get_order(int x) const {
        auto pos = pre_est_var_order.find(x);
        // std::cout << "check "<< x ;
        if (pos == pre_est_var_order.end()) {
        //    std::cout << " --> 0\n";
            return 0;
        }
        
        // std::cout << " -->" << pos->second<<"\n";
        return pos->second;
    }
};

#endif
