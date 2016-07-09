#pragma once

#include "index_common.hpp"
#include <sdsl/suffix_arrays.hpp>
#include <sdsl/bp_support.hpp>
#include <sdsl/rmq_support.hpp>

namespace topkcomp {

template<typename t_csa = sdsl::csa_wt<>,
         typename t_bv = sdsl::sd_vector<>,
         typename t_rnk= typename t_bv::rank_1_type,
         typename t_sel= typename t_bv::select_1_type,
         typename t_rmq = sdsl::rmq_succinct_sct<0>>
class index5 {
    typedef sdsl::int_vector<8> t_label;
    typedef edge_rac<t_label>   t_edge_label;

    t_csa              m_csa;       // CSA of concatenation of strings
    t_bv               m_start;     // marks starts of strings in CSA
    t_rnk              m_start_rnk; // rank support structure for m_start
    t_sel              m_start_sel; // select support structure for m_start
    sdsl::int_vector<> m_weight;    // weights of strings
    t_rmq              m_rmq;       // range maximum query on m_weight

    public:
        typedef size_t size_type;

        // Constructor takes a sorted list of (string,weight)-pairs
        index5(const tVPSU& string_weight=tVPSU()) {
            using namespace sdsl;
            if ( !string_weight.empty() ) {
                uint64_t N, n, max_weight;
                std::tie(N, n, max_weight) = input_stats(string_weight);
                // initialize m_weight
                m_weight = int_vector<>(N, 0, bits::hi(max_weight)+1);
                for (size_t i=0; i < N; ++i) {
                    m_weight[i] = string_weight[i].second;
                }
                // initialize range maximum structure
                m_rmq = t_rmq(&m_weight);
                // construct compressed suffix array
                auto concat_file = tmp_file(std::string("./"),"_index5");
                {
                    {
                        std::string concat;
                        for (auto ep : string_weight) {
                            concat.append(ep.first.begin(), ep.first.end());
                        }
                        store_to_file(concat.c_str(), concat_file);
                    }
                    construct(m_csa, concat_file, 1);
                    sdsl::remove(concat_file);
                }
                // use LF function to mark start of strings
                {
                    bit_vector bv(m_csa.size(), 0);
                    size_t sa_pos = 0;
                    for (size_t i = 0; i < string_weight.size(); ++i){
                        for (size_t j = 0; j < string_weight[string_weight.size()-i-1].first.size(); ++j) {
                            sa_pos = m_csa.lf[sa_pos];
                        }
                        bv[sa_pos] = 1;
                    }
                    m_start = t_bv(bv);
                    m_start_rnk = t_rnk(&m_start);
                    m_start_sel = t_sel(&m_start);
                }
            }
        }

        // k > 0
        tVPSU top_k(const std::string& prefix, size_t k) const{
            auto range = prefix_range(prefix);
            auto top_idx = heaviest_indexes_in_range(k, range, m_weight, m_rmq);
            tVPSU result_list;
            for (auto idx : top_idx){
                result_list.push_back(tPSU(label(idx), m_weight[idx]));
            }
            return result_list;
        }

        // Serialize method
        size_type
        serialize(std::ostream& out, sdsl::structure_tree_node* v=nullptr,
                  std::string name="") const {
            using namespace sdsl;
            auto child = structure_tree::add_child(v, name, util::class_name(*this));
            size_type written_bytes = 0;
            written_bytes += m_csa.serialize(out, child, "csa");
            written_bytes += m_start.serialize(out, child, "start");
            written_bytes += m_start_rnk.serialize(out, child, "start_rnk");
            written_bytes += m_start_sel.serialize(out, child, "start_sel");
            written_bytes += m_weight.serialize(out, child, "weight");
            written_bytes += m_rmq.serialize(out, child, "rmq");
            structure_tree::add_size(child, written_bytes);
            return written_bytes;
        }

        // Load method
        void load(std::istream& in) {
            m_csa.load(in);
            m_start.load(in);
            m_start_rnk.load(in);
            m_start_rnk.set_vector(&m_start);
            m_start_sel.load(in);
            m_start_sel.set_vector(&m_start);
            m_weight.load(in);
            m_rmq.load(in);
        }

    private:

        // Return range [lb, rb) of matching entries
        std::array<size_t,2> prefix_range(const std::string& prefix) const {
            auto sa_range = lex_interval(m_csa, prefix.begin(), prefix.end());
            return {{m_start_rnk(sa_range[0]), m_start_rnk(sa_range[1]+1)}};
        }

        std::string label(size_t idx) const {
            size_t sa_pos = m_start_sel(idx+1);
            std::string res;
            do {
                res.append(1, first_row_symbol(sa_pos, m_csa));
                sa_pos = m_csa.psi[sa_pos];
            } while ( !m_start[sa_pos] );
            return res; 
        }
};

} // end namespace topkcomp
