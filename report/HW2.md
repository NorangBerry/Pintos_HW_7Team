##  문제
Synchronization 해결을 위한 priority scheduler 구성

## 아이디어
thread에 우선순위를 부여하여 우선순위에 따라 스레드 실행
lock에 의한 priority donation 구현

## 구현
### synch.c
#### compare function 구성
thread와 lock과 semaphore의 priority를 비교하는 function 생성

#### sema_up function 수정
1. sema waiter 에서 max priority를 가진 thread 선택
2. 기존 thread와 priority 비교   
#### lock에 initial maxpriority 추가
#### lock_acquire에 priority donation 추가
1. lock을 쥐고있는 holder보다 현재 thread의 priority가 높으면 holder와 lock의 priority를 현재 thread의 priority로 바꿈\
2. holder가 다른 lock을 기다리는지 check

#### lock release 수정
1. lock이 마지막이면, thread의 priority를 원래대로 변경
2. lock이 계속되면, priority를 비교하여 변경

#### cond_wait 수정
1. semaphore 비교해서 inserted order로 정렬
