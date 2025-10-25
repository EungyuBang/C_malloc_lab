# 📌 malloc-lab ROADMAP

**점수 기준**:  
- 메모리 효율(Memory Utilization)  
- 속도(Throughput)

---

## 🔹 점수 변화 요소
1. **배치 정책(Placement Policy)**  
2. **병합 정책(Coalescing Policy)**  
3. **가용 리스트(Free List) 방식**

---

## 1️⃣ 암묵적 가용 리스트 + First Fit + 즉시 병합
- **책 코드 그대로 구현**  
- **점수 구분**:  
  - 44 (utils) + 26 (thru)
  - **총합: 70 ~ 72 / 100**  

---

## 2️⃣ 암묵적 가용 리스트 + Next Fit + 즉시 병합
- **장점**: First Fit보다 탐색 속도 빠름  
- **점수 예상**: TBD

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


