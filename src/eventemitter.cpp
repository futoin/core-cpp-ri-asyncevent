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

#include <futoin/fatalmsg.hpp>
#include <futoin/ri/eventemitter.hpp>
//---
#include <future>
#include <vector>

namespace futoin {
    namespace ri {
        struct EventEmitter::Impl
        {
            Impl(IAsyncTool& async_tool) noexcept : async_tool(async_tool) {}

            struct EventInfo
            {
                EventInfo(
                        futoin::string&& name,
                        EventID eid,
                        TestCast tc,
                        const NextArgs& ma) noexcept :
                    name(std::forward<futoin::string>(name)),
                    event_id(eid),
                    test_cast(tc),
                    model_args(&ma)
                {}

                futoin::string name;
                EventID event_id;
                TestCast test_cast;
                const NextArgs* model_args;
                bool in_process{false};
                std::vector<EventHandler*> listeners;
                std::vector<EventHandler*> once;
            };

            EventInfo& get_event_info(
                    EventEmitter& ee, const EventType& et) noexcept
            {
                const auto event_id = Accessor::event_id(et);

                if (event_id == NO_EVENT_ID) {
                    // Slow path
                    futoin::string name{Accessor::raw_event_type(et)};

                    for (auto& er : events) {
                        if (er.name == name) {
                            return er;
                        }
                    }

                    FatalMsg() << "unknown event type: " << name;
                }

                if (Accessor::event_emitter(et) != &ee) {
                    FatalMsg() << "foreign event type!";
                }

                return events[event_id - 1];
            }

            EventInfo& process_new_handler(
                    EventEmitter& ee,
                    const EventType& et,
                    EventHandler& handler) noexcept
            {
                if (Accessor::event_id(handler) != NO_EVENT_ID) {
                    FatalMsg() << "handler re-use is not supported!";
                }

                auto& ei = get_event_info(ee, et);

                handler.test_cast()(*(ei.model_args));

                auto& handler_et = Accessor::event_type(handler);
                Accessor::event_id(handler_et) = ei.event_id;
                Accessor::event_emitter(handler_et) = &ee;
                return ei;
            }

            void call_listeners(
                    EventInfo& ei, const NextArgs& args = no_args) noexcept
            {
                if (ei.in_process) {
                    FatalMsg() << "emit() recursion for: " << ei.name;
                }

                ei.in_process = true;

                auto& listeners = ei.listeners;
                auto& once = ei.once;

                // NOTE: the listeners and once vector MAY change during
                // execution!
                const auto listener_size = listeners.size();
                const auto once_size = once.size();

                // Run through persistent listeners
                for (std::size_t i = 0; i < listener_size; ++i) {
                    auto hp = listeners[i];

                    if (hp != nullptr) {
                        (*hp)(args);
                    }
                }

                // Process once
                for (std::size_t i = 0; i < once_size; ++i) {
                    auto hp = once[i];

                    if (hp != nullptr) {
                        Accessor::event_id(*hp) = NO_EVENT_ID;
                        (*hp)(args);
                    }
                }

                if (once_size == once.size()) {
                    once.clear();
                } else {
                    // Leave new handlers appeared after this call
                    once.erase(once.begin(), once.begin() + once_size);
                }

                //---
                ei.in_process = false;
            }

            IAsyncTool& async_tool;
            SizeType max_listeners{8};
            std::vector<EventInfo> events;
            static const NextArgs no_args;
        };

        const EventEmitter::NextArgs EventEmitter::Impl::no_args;

        EventEmitter::EventEmitter(IAsyncTool& async_tool) noexcept :
            impl_(new Impl(async_tool))
        {}

        EventEmitter::~EventEmitter() noexcept = default;

#define ENSURE_IN_EVENT_LOOP(call_details)        \
    if (!impl_->async_tool.is_same_thread()) {    \
        std::promise<void> done;                  \
        auto f = [&, this]() {                    \
            this->call_details;                   \
            done.set_value();                     \
        };                                        \
                                                  \
        impl_->async_tool.immediate(std::ref(f)); \
        done.get_future().wait();                 \
        return;                                   \
    }

        void EventEmitter::register_event_impl(
                EventType& event,
                TestCast test_cast,
                const NextArgs& model_args) noexcept
        {
            ENSURE_IN_EVENT_LOOP(
                    register_event_impl(event, test_cast, model_args));

            if (Accessor::event_id(event) != 0) {
                FatalMsg() << "Re-use of EventType object on registration";
            }

            futoin::string name{Accessor::raw_event_type(event)};
            auto& events = impl_->events;

            for (auto& er : events) {
                if (er.name == name) {
                    FatalMsg() << "Double registration of event: " << name;
                }
            }

            events.emplace_back(
                    std::move(name), events.size() + 1, test_cast, model_args);
            Accessor::event_id(event) = events.size();
            Accessor::event_emitter(event) = this;
        }

        void EventEmitter::setMaxListeners(
                EventEmitter& ee, SizeType max_listeners) noexcept
        {
            ee.impl_->max_listeners = max_listeners;
        }

        void EventEmitter::on(
                const EventType& event, EventHandler& handler) noexcept
        {
            ENSURE_IN_EVENT_LOOP(on(event, handler));

            auto& ei = impl_->process_new_handler(*this, event, handler);
            auto& listeners = ei.listeners;

            if (!ei.in_process) {
                for (auto& hp : listeners) {
                    if (hp == nullptr) {
                        hp = &handler;
                        return;
                    }
                }
            }

            if (listeners.size() == impl_->max_listeners) {
                FatalMsgHook::stream()
                        << "WARN: reached max event listeners: " << ei.name
                        << std::endl;
            }

            listeners.emplace_back(&handler);
        }

        void EventEmitter::once(
                const EventType& event, EventHandler& handler) noexcept
        {
            ENSURE_IN_EVENT_LOOP(once(event, handler));

            auto& ei = impl_->process_new_handler(*this, event, handler);
            auto& once = ei.once;

            if (once.size() == impl_->max_listeners) {
                FatalMsgHook::stream()
                        << "WARN: reached max event once listeners: " << ei.name
                        << std::endl;
            }

            once.emplace_back(&handler);
        }

        void EventEmitter::off(
                const EventType& event, EventHandler& handler) noexcept
        {
            ENSURE_IN_EVENT_LOOP(off(event, handler));

            auto& ei = impl_->get_event_info(*this, event);

            bool found = false;
            auto& listeners = ei.listeners;

            for (auto& hp : listeners) {
                if (hp == &handler) {
                    found = true;
                    hp = nullptr;
                    break;
                }
            }

            if (!found) {
                auto& once = ei.once;

                for (auto& hp : once) {
                    if (hp == &handler) {
                        found = true;
                        hp = nullptr;
                        break;
                    }
                }
            }

            if (found) {
                Accessor::event_id(handler) = NO_EVENT_ID;
            } else {
                FatalMsg() << "Not registered handler!";
            }
        }

        void EventEmitter::emit(const EventType& event) noexcept
        {
            ENSURE_IN_EVENT_LOOP(emit(event));

            auto& ei = impl_->get_event_info(*this, event);
            impl_->call_listeners(ei);
        }

        void EventEmitter::emit(
                const EventType& event, NextArgs&& args) noexcept
        {
            ENSURE_IN_EVENT_LOOP(emit(event, std::forward<NextArgs>(args)));

            auto& ei = impl_->get_event_info(*this, event);
            ei.test_cast(args);
            impl_->call_listeners(ei, args);
        }
    } // namespace ri
} // namespace futoin
