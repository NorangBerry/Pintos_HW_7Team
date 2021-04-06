문제
======

현재 sleep은 busy waiting으로 구현되어있다. 이것을 busy waiting이 아니게 바꾸어라.<br>
테스트 코드를 분석해보면, 위 문장은 테스트의 결과값으로 출력되는 idle ticks을 0이 아니게, kernel ticks를 0에 가깝게 만들어라는 말과 동일한 말입니다. 따라서 어떻게 하면 idle ticks를 늘릴 수 있는지에 대해서 파악했습니다.

문제 해결
======

#### ready_list를 비워야한다.

next_thread_to_run()함수에서 다음에 실행될 쓰레드를 정하는데, 여기서 idle_thread가 호출되어야 idle_ticks의 값이 증가하게 됩니다. 그렇다고 ready_list에서 그냥 빼버리면 그 쓰레드는 다시는 실행되지 않고, ready_list외에 준비된 list도 없었습니다. 따라서 별도의 리스트인 waiting_list를 만들어서 저장하도록 했습니다. 그리고 ready_list에 넣는 함수 thread_yeild와 구분되도록 thread_wait 함수를 추가하였습니다.

#### waiting_list에서 꺼내는데는 순서가 있어야한다.

단순히 waiting_list에 큐 형식(FIFO)으로 넣게되면 sleep한 시간이 짧은 함수가 sleep 시간이 긴 함수 뒤에 배치될 수 있습니다. 여기서 두가지 해결책이 있었습니다. 첫번째는 리스트를 ordered_queue로 만드는 것이고 두번째는 단순 list로 생각하고 꺼낼 때 priority가 높은 것(= 남은 sleep 시간이 짧은 것)을 꺼내는 것이었습니다. 하지만, 만약 두번째로 구현하게되면 매번 waiting_list를 확인하는데에 O(n)의 시간이 소모됩니다. 따라서, 큐에 넣는데에 O(n)의 시간이 걸리는 첫번째 방법으로 구현하였습니다.

#### waiting_list에 넣는 로직이 있으면, 꺼내는 로직이 있어야한다.

이것도 두가지 타이밍이 있다고 생각했습니다. 첫번째는 실행할 쓰레드를 고르는 시점, 두번째는 정기적으로 일어나는 time_interrupt가 호출되는 시점입니다. 일정 시간동안 sleep을 시키는 함수이므로, 시간에 맞춰서 일어나는 time_interrupt에서 sleep을 하는 것이 더 정확할 것이라고 생각하여 이 쪽으로 구현하였습니다.<br>
또 한가지 문제가 더 있었는데, include 문제였습니다. 쓰레드 교체 시점에서는 timer.c의 함수를 실행할 방법이 없었습니다. #include를 통해서 timer의 함수를 가져올 수 있었지만 이것은 threads폴더에서 devices폴더의 파일을 포함하는 것입니다. 제가 파악한 바로는 threads폴더가 devices폴더보다 개념적으로 상위 레이어에 존재하는 폴더로, 이와 같은 include 호출은 프로젝트 코드를 엉망으로 만들 수 있다고 판단하였습니다.

#### 그 외 주요 포인트

1. ready_list랑 명확히 구분하기 위해서 THREAD_WAIT state를 추가하였습니다.
2. waiting_list에서 언제 꺼내야하는지 판단할 수 있어야하므로, thread의 변수에 wait_tick을 추가하였습니다.
3. tick은 오버플로가 일어날 수 있는 것으로 생각되어, timer_elapsed 함수를 통해서 비교하는 방식으로 구현하였습니다.


추가된 사항 정리
======

1. sleep 함수의 수정
2. tick_compare 함수의 추가
3. THREAD_WAIT 변수 추가 및 print_stacktrace함수에서 THREAD_WAIT 케이스 추가
4. waiting_list 변수 추가 및 thread_init에서 초기화
5. waiting_list를 관리하는 thread_wait, awake_thread 추가
6. 함수 포인터를 쉽게 넘기기 위해 tick_calc_func 이름을 typedef로 추가