# 📌 malloc-lab ROADMAP

**점수 기준**:  
- 메모리 효율(Memory Utilization)  
- 속도(Throughput)

---

## 🔹 점수 변화 요소
1. **배치 정책(Placement Policy)**  
  - First Fit
  - Next fit
  - Best fit

2. **병합 정책(Coalescing Policy)**  
  - 즉시 병합
  - 지연 병합

3. **가용 리스트(Free List) 방식**
  - Implicit 
  - Explicit 
  - Segreated 

---

## 1️⃣ 암묵적 가용 리스트 + First Fit + 즉시 병합
- **bookcode.c** 
- **책 코드 그대로 구현**  
- **점수 구분**:  
  - 44 (utils) + 26 (thru)
  - **총합: 70 ~ 72 / 100**  

---

## 2️⃣ 암묵적 가용 리스트 + Next Fit + 즉시 병합
- **nextfit.c**
- **로직 생각**  
  - First fit 과 다르게 이전 탐색이 어디서 끝난지에 대한 정보가 있어야 함   
  - 탐색이 끝났던 곳? -> 이전 가용공간이 할당된 곳! 
  - 변수 만들어서 위치 관리, 업데이트
  - 1. 우선 현재 위치부터 힙의 끝까지 탐색 -> 있어? -> 거기 할당
  - 2. 힙 끝까지 갔는데도 없어? -> 다시 처음부터 탐색 
  - 3. 처음부터 탐색했는데도 없어? -> NULL 반환 -> extend_heap 호출!
- **점수 구분**: 
  - 44 (utils) + 40 (thru)
  - **총합: 83 ~ 84 / 100**

---

## 3️⃣ 명시적 가용 리스트(LIFO) + free list 안에서 first fit + 즉시 병합
- **explicit.c**
- **로직 생각**
  - 연결 방법 -> LIFO, 주소 순서 방식 있음  - 우선 LIFO 로 구현
  - 우선 free 공간 payload 안에 pred(이전 가용 블록 주소), succ(다음 가용 블록 주소) 포인터 넣어야 함 -> 그럼 free 블록 최소 크기가 Header + Footer + pred + succ
  - 그 다음 insert_free_list , delete_free_list 만들어서 free 할때 insert_free_list 해야되고, 할당될때 delete_free_list 만들어야 함
  - 그리고 Next Fit 필요 없음 -> 어차피 가용된 애들만 따로 관리하기 때문에 그 안에서 적합한거 바로 할당해도 됨
  - LIFO 로 구현할거니까... free list 맨 앞에 제일 최근 free된 블록 삽입
- **점수 예상**: TBD

---

## 4️⃣ 명시적 가용 리스트 + free list 안에서 first fit + 즉시 병합 + Footer 최적화
- **장점**:  
  - Footer 접근 최소화 → Overhead 감소  
  - 병합/검색 효율 향상  
- **점수 예상**: TBD

---


