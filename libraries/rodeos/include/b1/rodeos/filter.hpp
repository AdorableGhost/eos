#pragma once

#include <b1/rodeos/callbacks/basic.hpp>
#include <b1/rodeos/callbacks/chaindb.hpp>
#include <b1/rodeos/callbacks/compiler_builtins.hpp>
#include <b1/rodeos/callbacks/console.hpp>
#include <b1/rodeos/callbacks/filter.hpp>
#include <b1/rodeos/callbacks/memory.hpp>
#include <b1/rodeos/callbacks/unimplemented.hpp>
#include <b1/rodeos/callbacks/unimplemented_filter.hpp>
#include <boost/hana/for_each.hpp>

// todo: configure limits
// todo: timeout
namespace b1::rodeos::filter {

struct callbacks;
using rhf_t     = registered_host_functions<callbacks>;
using backend_t = eosio::vm::backend<rhf_t, eosio::vm::jit>;

#ifdef EOSIO_EOS_VM_OC_RUNTIME_ENABLED
struct eosvmoc_tier {
   eosvmoc_tier(const boost::filesystem::path& d, const eosio::chain::webassembly::eosvmoc::config& c,
                const std::vector<uint8_t>& code, const eosio::chain::digest_type& code_hash)
       : cc(d, c,
            [code, code_hash](const eosio::chain::digest_type& id, uint8_t vm_version) -> std::string_view {
               if (id == code_hash)
                  return { reinterpret_cast<const char*>(code.data()), code.size() };
               else
                  return {};
            }),
         exec(cc), mem(512, get_intrinsic_map()), hash(code_hash) {
      // start background compile
      cc.get_descriptor_for_code(code_hash, 0);
   }
   eosio::chain::webassembly::eosvmoc::code_cache_async cc;
   eosio::chain::webassembly::eosvmoc::executor         exec;
   eosio::chain::webassembly::eosvmoc::memory           mem;
   eosio::chain::digest_type                            hash;
};
#endif

struct filter_state : b1::rodeos::data_state<backend_t>, b1::rodeos::console_state, b1::rodeos::filter_callback_state {
   eosio::vm::wasm_allocator wa = {};
#ifdef EOSIO_EOS_VM_OC_RUNTIME_ENABLED
   std::optional<eosvmoc_tier> eosvmoc_tierup;
#endif
};

struct callbacks : b1::rodeos::chaindb_callbacks<callbacks>,
                   b1::rodeos::compiler_builtins_callbacks<callbacks>,
                   b1::rodeos::console_callbacks<callbacks>,
                   b1::rodeos::context_free_system_callbacks<callbacks>,
                   b1::rodeos::data_callbacks<callbacks>,
                   b1::rodeos::db_callbacks<callbacks>,
                   b1::rodeos::filter_callbacks<callbacks>,
                   b1::rodeos::memory_callbacks<callbacks>,
                   b1::rodeos::unimplemented_callbacks<callbacks>,
                   b1::rodeos::unimplemented_filter_callbacks<callbacks> {
   filter::filter_state&      filter_state;
   b1::rodeos::chaindb_state& chaindb_state;
   b1::rodeos::db_view_state& db_view_state;

   callbacks(filter::filter_state& filter_state, b1::rodeos::chaindb_state& chaindb_state,
             b1::rodeos::db_view_state& db_view_state)
       : filter_state{ filter_state }, chaindb_state{ chaindb_state }, db_view_state{ db_view_state } {}

   auto& get_state() { return filter_state; }
   auto& get_filter_callback_state() { return filter_state; }
   auto& get_chaindb_state() { return chaindb_state; }
   auto& get_db_view_state() { return db_view_state; }
};

template <typename S>
inline void unknown_intrinsic(...) {
   std::cerr << "Unknown intrinsic: " << S().c_str() << std::endl;
}

inline void register_callbacks() {
   b1::rodeos::chaindb_callbacks<callbacks>::register_callbacks<rhf_t>();
   b1::rodeos::compiler_builtins_callbacks<callbacks>::register_callbacks<rhf_t>();
   b1::rodeos::console_callbacks<callbacks>::register_callbacks<rhf_t>();
   b1::rodeos::context_free_system_callbacks<callbacks>::register_callbacks<rhf_t>();
   b1::rodeos::data_callbacks<callbacks>::register_callbacks<rhf_t>();
   b1::rodeos::db_callbacks<callbacks>::register_callbacks<rhf_t>();
   b1::rodeos::filter_callbacks<callbacks>::register_callbacks<rhf_t>();
   b1::rodeos::memory_callbacks<callbacks>::register_callbacks<rhf_t>();
   b1::rodeos::unimplemented_callbacks<callbacks>::register_callbacks<rhf_t>();
   b1::rodeos::unimplemented_filter_callbacks<callbacks>::register_callbacks<rhf_t>();

#ifdef EOSIO_EOS_VM_OC_RUNTIME_ENABLED
   const auto& base_map = eosio::chain::webassembly::eosvmoc::get_intrinsic_map();
   auto&       my_map   = get_intrinsic_map();

   // Internal intrinsics do not use apply_context and are therefore safe to use as-is
   for (const auto& item : base_map) {
      if (item.first.substr(0, 16) == "eosvmoc_internal" || item.first.substr(0, 15) == "eosio_injection" || item.first == "env.eosio_exit") {
         my_map.insert(item);
      }
   }

   boost::hana::for_each(eosio::chain::webassembly::eosvmoc::intrinsic_table, [&](auto S) {
      my_map.insert(
            { S.c_str(),
              { nullptr, reinterpret_cast<void*>(unknown_intrinsic<decltype(S)>),
                ::boost::hana::index_if(eosio::chain::webassembly::eosvmoc::intrinsic_table, ::boost::hana::equal.to(S))
                      .value() } });
   });
#endif
}

} // namespace b1::rodeos::filter
