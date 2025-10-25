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
- **책 코드 그대로 구현**  
- **점수 구분**:  
  - 44 (utils) + 26 (thru)
  - **총합: 70 ~ 72 / 100**  

---

## 2️⃣ 암묵적 가용 리스트 + Next Fit + 즉시 병합
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

## 3️⃣ 명시적 가용 리스트 + Next Fit + 즉시 병합
- **장점**: 메모리 활용 효율 개선  
- **점수 예상**: TBD

---

## 4️⃣ 명시적 가용 리스트 + Next Fit + 즉시 병합 + Footer 최적화
- **장점**:  
  - Footer 접근 최소화 → Overhead 감소  
  - 병합/검색 효율 향상  
- **점수 예상**: TBD

---


