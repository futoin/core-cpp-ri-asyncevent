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

#include <boost/test/unit_test.hpp>
//---
#include <atomic>
#include <deque>
#include <future>
//---
#include <futoin/ri/asynctool.hpp>
#include <futoin/ri/eventemitter.hpp>

BOOST_AUTO_TEST_SUITE(eventemitter) // NOLINT

struct TestEventEmitter : futoin::ri::EventEmitter
{
    TestEventEmitter(futoin::ri::AsyncTool& at) : EventEmitter(at) {}

    using EventEmitter::register_event;
};

futoin::ri::AsyncTool at;

void wait_at_halt()
{
    std::promise<void> done;
    at.immediate([&]() { done.set_value(); });
    done.get_future().wait();
}

BOOST_AUTO_TEST_CASE(instance) // NOLINT
{
    TestEventEmitter tee{at};
}

BOOST_AUTO_TEST_CASE(register_event) // NOLINT
{
    TestEventEmitter tee{at};

    TestEventEmitter::EventType test_event("TestEvent");
    tee.register_event(test_event);
}

BOOST_AUTO_TEST_CASE(on) // NOLINT
{
    TestEventEmitter tee{at};
    futoin::IEventEmitter& ee = tee;

    TestEventEmitter::EventType test_event("TestEvent");
    tee.register_event(test_event);

    TestEventEmitter::EventHandler handler([]() {});
    ee.on(test_event, handler);
    ee.off(test_event, handler);
}

BOOST_AUTO_TEST_CASE(once) // NOLINT
{
    TestEventEmitter tee{at};
    futoin::IEventEmitter& ee = tee;

    TestEventEmitter::EventType test_event("TestEvent");
    tee.register_event(test_event);

    TestEventEmitter::EventHandler handler([]() {});
    ee.once(test_event, handler);
    ee.off(test_event, handler);
}

BOOST_AUTO_TEST_CASE(emit) // NOLINT
{
    TestEventEmitter tee{at};
    futoin::IEventEmitter& ee = tee;

    TestEventEmitter::EventType test_event("TestEvent");
    tee.register_event(test_event);

    ee.emit(test_event);

    wait_at_halt();
}

BOOST_AUTO_TEST_CASE(with_args) // NOLINT
{
    TestEventEmitter tee{at};
    futoin::IEventEmitter& ee = tee;
    std::atomic_size_t count{0};

    TestEventEmitter::EventType test_event1("TestEvent1");
    tee.register_event<int>(test_event1);
    ee.emit(test_event1, 123);

    TestEventEmitter::EventHandler handler1([&](int a) {
        BOOST_CHECK_EQUAL(a, 123);
        ++count;
    });
    ee.on(test_event1, handler1);
    ee.emit(test_event1, 123);
    wait_at_halt();
    ee.off(test_event1, handler1);

    ee.once(test_event1, handler1);
    ee.emit(test_event1, 123);
    ee.emit(test_event1, 234);
    wait_at_halt();

    TestEventEmitter::EventType test_event2("TestEvent2");
    tee.register_event<int, futoin::string>(test_event2);

    TestEventEmitter::EventHandler handler2(
            [&](int a, const futoin::string& b) {
                BOOST_CHECK_EQUAL(a, 123);
                BOOST_CHECK_EQUAL(b, "str");
                ++count;
            });
    ee.on(test_event2, handler2);
    ee.emit(test_event2, 123, "str");
    wait_at_halt();
    ee.off(test_event2, handler2);
    ee.once(test_event2, handler2);
    ee.emit(test_event2, 123, futoin::string{"str"});

    TestEventEmitter::EventType test_event3("TestEvent3");
    tee.register_event<int, futoin::string, std::vector<int>>(test_event3);
    TestEventEmitter::EventHandler handler3(
            [&](int a, const futoin::string& b, const std::vector<int>& c) {
                BOOST_CHECK_EQUAL(a, 123);
                BOOST_CHECK_EQUAL(b, "str");
                BOOST_CHECK_EQUAL(c[0], 1);
                BOOST_CHECK_EQUAL(c[1], 2);
                BOOST_CHECK_EQUAL(c[2], 3);
                ++count;
            });
    ee.on(test_event3, handler3);
    ee.emit(test_event3, 123, "str", std::vector<int>{1, 2, 3});
    wait_at_halt();
    ee.off(test_event3, handler3);
    ee.once(test_event3, handler3);
    ee.emit(test_event3, 123, "str", std::vector<int>{1, 2, 3});

    TestEventEmitter::EventType test_event4("TestEvent4");
    tee.register_event<int, futoin::string, std::vector<int>, bool>(
            test_event4);
    TestEventEmitter::EventHandler handler4([&](int a,
                                                const futoin::string& b,
                                                const std::vector<int>& c,
                                                const bool d) {
        BOOST_CHECK_EQUAL(a, 123);
        BOOST_CHECK_EQUAL(b, "str");
        BOOST_CHECK_EQUAL(c[0], 1);
        BOOST_CHECK_EQUAL(c[1], 2);
        BOOST_CHECK_EQUAL(c[2], 3);
        BOOST_CHECK_EQUAL(d, true);
        ++count;
    });
    ee.on(test_event4, handler4);
    ee.emit(test_event4, 123, "str", std::vector<int>{1, 2, 3}, true);
    wait_at_halt();
    ee.off(test_event4, handler4);
    ee.once(test_event4, handler4);
    ee.emit(test_event4, 123, "str", std::vector<int>{1, 2, 3}, true);

    wait_at_halt();
    BOOST_CHECK_EQUAL(count.load(), 8U);
}

BOOST_AUTO_TEST_CASE(multiple) // NOLINT
{
    TestEventEmitter tee{at};
    futoin::IEventEmitter& ee = tee;
    std::atomic_size_t count{0};
    const size_t HCOUNT = 10;

    futoin::IEventEmitter::EventType test_event{"TestEvent"};
    tee.register_event<int, futoin::string>(test_event);

    auto handler = [&](int a) {
        BOOST_CHECK_EQUAL(a, 123);
        ++count;
    };

    std::deque<futoin::IEventEmitter::EventHandler> handlers;

    for (auto i = HCOUNT; i > 0; --i) {
        handlers.emplace_back(std::ref(handler));
        ee.on("TestEvent", handlers.back());

        handlers.emplace_back(std::ref(handler));
        ee.on("TestEvent", handlers.back());

        handlers.emplace_back(std::ref(handler));
        ee.once(test_event, handlers.back());

        handlers.emplace_back(std::ref(handler));
        ee.once(test_event, handlers.back());

        handlers.emplace_back(std::ref(handler));
        ee.once(test_event, handlers.back());
    }

    at.immediate([&]() {
        ee.emit(test_event, 123, "str");
        ee.emit(test_event, 123, "str");
        ee.emit(test_event, 123, "str");
    });

    wait_at_halt();
    BOOST_CHECK_EQUAL(count.load(), HCOUNT * (6 + 3));
}

BOOST_AUTO_TEST_CASE(edge_cases) // NOLINT
{
    TestEventEmitter tee{at};
    futoin::IEventEmitter& ee = tee;
    std::atomic_size_t count{0};

    futoin::IEventEmitter::EventType test_event{"TestEvent"};
    tee.register_event<int, futoin::string>(test_event);

    auto handler = [&](int a) {
        BOOST_CHECK_EQUAL(a, 123);
        ++count;
    };

    std::deque<futoin::IEventEmitter::EventHandler> handlers;

    handlers.emplace_back(std::ref(handler));
    ee.on("TestEvent", handlers.back());

    auto f = [&]() {
        ee.emit(test_event, 123, "str");

        handlers.emplace_back(std::ref(handler));
        ee.on("TestEvent", handlers.back());
        handlers.emplace_back(std::ref(handler));
        ee.once("TestEvent", handlers.back());

        at.immediate([&]() { BOOST_CHECK_EQUAL(count.load(), 1U); });

        ee.emit(test_event, 123, "str");

        at.immediate([&]() { BOOST_CHECK_EQUAL(count.load(), 4U); });

        ee.emit(test_event, 123, "str");
        handlers.emplace_back(std::ref(handler));
        ee.once("TestEvent", handlers.back());
        handlers.emplace_back(std::ref(handler));
        ee.once("TestEvent", handlers.back());

        at.immediate([&]() { BOOST_CHECK_EQUAL(count.load(), 6U); });

        ee.emit(test_event, 123, "str");
        handlers.emplace_back(std::ref(handler));
        ee.once("TestEvent", handlers.back());

        at.immediate([&]() { BOOST_CHECK_EQUAL(count.load(), 10U); });
    };

    at.immediate(std::ref(f));

    wait_at_halt();
    BOOST_CHECK_EQUAL(count.load(), 10U);
}

BOOST_AUTO_TEST_CASE(stress) // NOLINT
{
    struct TestData
    {
        TestData(futoin::ri::AsyncTool& at) noexcept : tee(at) {}

        TestEventEmitter tee;
        futoin::IEventEmitter& ee = tee;
        std::size_t count{0};
        std::promise<size_t> final_count;
        bool done{false};

        std::deque<futoin::IEventEmitter::EventHandler> handlers;
        std::deque<futoin::IEventEmitter::EventHandler> once_handlers;
        std::size_t once_next{0};

        std::function<void(int)> simple;
        std::function<void(int)> once_add;
        static void once() noexcept {}
        std::function<void()> emit;
    };

    TestData td(at);

    td.simple = [&](int) { ++(td.count); };
    td.once_add = [&](int) {
        td.ee.once("TestEvent", td.once_handlers[td.once_next]);

        ++(td.once_next);
        td.once_next %= td.once_handlers.size();
    };

    futoin::IEventEmitter::EventType test_event{"TestEvent"};
    td.tee.register_event<int, futoin::string>(test_event);
    TestEventEmitter::setMaxListeners(td.tee, 1000);

    td.emit = [&]() {
        if (td.done) {
            td.final_count.set_value(td.count);
        } else {
            td.ee.emit(test_event, 123, futoin::string{});
            at.immediate(std::ref(td.emit));
        }
    };

    at.immediate([&]() {
        for (auto i = 0; i < 100; ++i) {
            td.handlers.emplace_back(std::ref(td.simple));
            td.ee.on("TestEvent", td.handlers.back());

            td.handlers.emplace_back(std::ref(td.once_add));
            td.ee.on("TestEvent", td.handlers.back());

            td.once_handlers.emplace_back(&TestData::once);
            td.once_handlers.emplace_back(&TestData::once);
        }

        at.deferred(std::chrono::seconds(1), [&]() { td.done = true; });

        td.emit();
    });

    auto count = td.final_count.get_future().get();
    std::cout << "Stress count: " << count << std::endl;
    BOOST_CHECK_GT(count, size_t(1e5));
}

BOOST_AUTO_TEST_CASE(performance) // NOLINT
{
    struct TestData
    {
        TestData(futoin::ri::AsyncTool& at) noexcept : tee(at) {}

        TestEventEmitter tee;
        futoin::IEventEmitter& ee = tee;
        std::size_t count{0};
        std::promise<size_t> final_count;
        bool done{false};

        futoin::IEventEmitter::EventHandler handler;

        std::function<void(int)> simple;
        std::function<void()> emit;
    };

    TestData td(at);

    td.simple = [&](int) { ++(td.count); };

    futoin::IEventEmitter::EventType test_event{"TestEvent"};
    td.tee.register_event<int>(test_event);

    td.emit = [&]() {
        if (td.done) {
            td.final_count.set_value(td.count);
        } else {
            for (auto i = 0; i < 1000; ++i) {
                td.ee.emit(test_event, 123);
            }
            at.immediate(std::ref(td.emit));
        }
    };

    at.immediate([&]() {
        td.handler = std::ref(td.simple);
        td.ee.on("TestEvent", td.handler);

        at.deferred(std::chrono::seconds(1), [&]() { td.done = true; });

        td.emit();
    });

    auto count = td.final_count.get_future().get();
    std::cout << "Performance count: " << count << std::endl;
    BOOST_CHECK_GT(count, size_t(1e5));
}

BOOST_AUTO_TEST_SUITE_END() // NOLINT
