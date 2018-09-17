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
#include <cassert>
#include <deque>
#include <future>
#include <limits>

namespace futoin {
    namespace ri {
        struct EventEmitter::Impl
        {
            using Listeners = std::deque<EventHandler*>;
            using ListenerSize = std::uint16_t;

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
                Listeners listeners;
                Listeners once;
                ListenerSize once_next{0};
                ListenerSize pending{0};
                bool in_process{false};
            };

            struct EmitTask
            {
                EmitTask(EventInfo& ei, NextArgs&& args) noexcept :
                    listeners_count(ei.listeners.size()),
                    once_count(ei.once_next),
                    args(std::forward<NextArgs>(args)),
                    event_info(ei)
                {
                    ei.once_next = 0;
                }

                void operator()() noexcept
                {
                    event_info.in_process = true;

                    // NOTE: iterators get invalidated!

                    // Run through persistent listeners
                    auto& listeners = event_info.listeners;

                    for (ListenerSize i = 0; i < listeners_count; ++i) {
                        auto hp = listeners[i];

                        if (hp != nullptr) {
                            (*hp)(args);
                        }
                    }

                    // Process once
                    if (once_count > 0) {
                        auto& once = event_info.once;

                        for (ListenerSize i = 0; i < once_count; ++i) {
                            auto hp = once[i];

                            if (hp != nullptr) {
                                Accessor::event_id(*hp) = NO_EVENT_ID;
                                (*hp)(args);
                            }
                        }

                        // Leave new handlers appeared after this call
                        once.erase(once.begin(), once.begin() + once_count);
                    }

                    //---
                    --(event_info.pending);
                    event_info.in_process = false;
                }

                const ListenerSize listeners_count;
                const ListenerSize once_count;
                const NextArgs args;
                EventInfo& event_info;
            };

            Impl(IAsyncTool& async_tool) noexcept : async_tool(async_tool) {}
            ~Impl() noexcept
            {
                if (!tasks.empty()) {
                    FatalMsg()
                            << "EventEmitter destruction with pending tasks!";
                }
            }

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

            void call_listeners(EventInfo& ei, NextArgs&& args = {}) noexcept
            {
                if (ei.in_process) {
                    FatalMsg() << "emit() recursion for: " << ei.name;
                }

                if (ei.listeners.empty() && ei.once.empty()) {
                    return;
                }

                assert(ei.pending != std::numeric_limits<ListenerSize>::max());

                ++(ei.pending);

                tasks.emplace_back(ei, std::forward<NextArgs>(args));
                async_tool.immediate(std::ref(*this));
            }

            void operator()() noexcept
            {
                tasks.front()();
                tasks.pop_front();
            }

            IAsyncTool& async_tool;
            SizeType max_listeners{8};
            std::deque<EventInfo> events;
            std::deque<EmitTask> tasks;
        };

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

            if (ei.pending == 0) {
                for (auto& hp : listeners) {
                    if (hp == nullptr) {
                        hp = &handler;
                        return;
                    }
                }
            }

            assert(listeners.size()
                   != std::numeric_limits<Impl::ListenerSize>::max());

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

            assert(once.size()
                   != std::numeric_limits<Impl::ListenerSize>::max());

            if (once.size() == impl_->max_listeners) {
                FatalMsgHook::stream()
                        << "WARN: reached max event once listeners: " << ei.name
                        << std::endl;
            }

            once.emplace_back(&handler);
            ++(ei.once_next);
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
            impl_->call_listeners(ei, std::forward<NextArgs>(args));
        }
    } // namespace ri
} // namespace futoin
