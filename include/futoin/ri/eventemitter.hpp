//-----------------------------------------------------------------------------
//   Copyright 2018 FutoIn Project
//   Copyright 2018 Andrey Galkin
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
//-----------------------------------------------------------------------------
//! @file
//! @brief Implementation of FTN15 Native Event API
//! @sa https://specs.futoin.org/final/preview/ftn15_native_event.html
//-----------------------------------------------------------------------------

#ifndef FUTOIN_RI_EVENTEMITTER_HPP
#define FUTOIN_RI_EVENTEMITTER_HPP
//---
#include <futoin/iasynctool.hpp>
#include <futoin/ieventemitter.hpp>
//---
#include <memory>
//---

namespace futoin {
    namespace ri {
        /**
         * @brief Implementation of async EventEmitter
         */
        class EventEmitter : public IEventEmitter
        {
        public:
            static void setMaxListeners(
                    EventEmitter& ee, SizeType max_listeners) noexcept;

            void on(const EventType& event,
                    EventHandler& handler) noexcept override;
            void once(
                    const EventType& event,
                    EventHandler& handler) noexcept override;
            void off(
                    const EventType& event,
                    EventHandler& handler) noexcept override;
            void emit(const EventType& event) noexcept override;
            void emit(
                    const EventType& event, NextArgs&& args) noexcept override;

        protected:
            void register_event_impl(
                    EventType& event,
                    TestCast test_cast,
                    const NextArgs& model_args) noexcept override;

            EventEmitter(IAsyncTool& async_tool) noexcept;
            ~EventEmitter() noexcept override;

        private:
            struct Impl;
            std::unique_ptr<Impl> impl_;
        };
    } // namespace ri
} // namespace futoin

//---
#endif // FUTOIN_RI_EVENTEMITTER_HPP
