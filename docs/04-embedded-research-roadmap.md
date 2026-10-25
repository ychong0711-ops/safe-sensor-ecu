# `safe-sensor-ecu` → 독일 자동차 임베디드 대학원/석사논문/박사과정 확장 로드맵

> **목적**: 현재 프로젝트(`safe-sensor-ecu`)를 출발점으로, 독일 자동차 임베디드 분야의
> **Masterstudium → Masterarbeit → Promotion (Dr.-Ing.)** 까지 확장하는 실전 로드맵.
> 기존 `docs/01-ee-master-phd-action-plan.md`, `docs/02-german-grad-phd-roadmap.md`를 보완하며,
> 본 프로젝트의 기술 자산을 **연구 질문(Research Questions)**과 **출판(Publication)**으로 연결한다.

---

## 0. 현재 프로젝트가 제공하는 "연구 씨앗"

| 자산 | 위치 | 연구 가치 |
|---|---|---|
| Q2.13 FNN (6→32→3, 288 MAC, 716 B) | `sim/train_and_quantize.py`, `swc_rust_app/src/` | TinyML on constrained MCUs, 형식 검증 가능한 NN |
| Dual-Hall gradiometer + 1oo2 plausibility | `swc_rust_app/src/lib.rs` | 물리 기반 AI 안전 모니터링, ISO/IEC 27090 |
| C/Rust 하이브리드 파티션 스케치 | `bsw_c/`, `swc_rust_*` | Freedom from Interference, MPU isolation |
| HIL fault-injection harness | `hil_sim/hil_runner.c` | Zero-reset recovery, WCET 측정 |
| IBP formal bounds | `sim/train_and_quantize.py` | 인증 가능한 NN 출력 범위 |

**핵심 연구 질문 (RQ)** — 이 프로젝트를 논문으로 확장할 때 답해야 할 질문:

- **RQ1**: Q2.13 FNN의 정확도 손실 없이 (R² 0.934) MCU에서 < 5 µs 이내 추론이 가능한가? (WCET 보장)
- **RQ2**: Dual-Hall gradiometer가 near-field 자기 스푸핑을 100% 차단하면서 false-positive rate < 1%를 유지할 수 있는가?
- **RQ3**: C/Rust 파티션 경계에서 Freedom from Interference를 형식적으로 증명할 수 있는가? (MPU + Rust 타입 시스템)
- **RQ4**: HIL 에뮬레이션이 실제 target(Cortex-M33/AURIX) 동작을 얼마나 정확히 예측하는가?
- **RQ5**: 합성 데이터 기반 모델이 실제 센서/환경에서 얼마나 일반화되는가?

---

## Stage 0 — 현재 상태 (2026, 완료)

- [x] 개념 데모 구현 (FNN + PimGuard + MPU 스케치 + HIL)
- [x] 재현 가능한 빌드 (`make all` → PASS)
- [x] README 정직화 (concept demonstrator 포지셔닝)
- [x] HTML 사용자 설명서 (`docs/user-manual.html`)
- [ ] **다음 단계 진입 조건**: GitHub 공개 레포 + README/문서 정리 + 데모 영상

---

## Stage 1 — 포트폴리오 강화 (2026 Q4, 2–3개월)

**목표**: 독일 대학원 지원/HiWi 지원 시 "이 프로젝트를 5분 안에 이해시키는" 자료 완성.

### 작업 패키지

| # | 작업 | 산출물 | 우선순위 |
|---|---|---|---|
| 1.1 | GitHub 공개 레포 정리 (README, LICENSE, CI badge) | 공개 레포 | P0 |
| 1.2 | 데모 영상 (3–5분): `make all` 실행 → HIL PASS → 스푸핑 차단 시연 | YouTube/MP4 | P0 |
| 1.3 | 기술 블로그 2편 (영문): "Q2.13 TinyML on Cortex-M33", "C/Rust Hybrid ECU Partition" | Medium/Dev.to | P1 |
| 1.4 | LinkedIn 프로필 정비 + 독일 자동차 임베디드 키워드 최적화 | LinkedIn | P1 |
| 1.5 | `docs/04-...` (본 문서) + `docs/02-...` (기존) 통합 정리 | 문서 | P2 |

### 핵심 메시지 (지원 시 사용)
> "I built a reproducible concept demonstrator for a hardware-enforced C/Rust hybrid ECU,
> combining a Q2.13 fixed-point neural filter with formal IBP bounds and a host HIL
> fault-injection harness — 11,500 cycles, 0 missed deadlines in emulation."

---

## Stage 2 — 독일 Masterstudium 진입 (2027–2029)

### 2.1 추천 대학/Lehrstuhl (자동차 임베디드 특화)

| 대학 | 프로그램 | 강점 | 비고 |
|---|---|---|---|
| **TU München (TUM)** | M.Sc. Electrical Engineering / Robotics | AUTOSAR, 임베디드 시스템, ECU | 경쟁 높음, 영어 프로그램 있음 |
| **KIT (Karlsruhe)** | M.Sc. Electrical Engineering | 자동차 전자, ECU, ADAS | 산업 연계 강함 |
| **TU Braunschweig** | M.Sc. Electrical Engineering | 차량 전자/통신, ADAS | 자동차 산업 근접 |
| **RWTH Aachen** | M.Sc. Electrical Engineering | 임베디드, 자동차, ICT | 연구 중심, PhD 연계 |
| **TU Dresden** | M.Sc. Electrical Engineering | 임베디드, IoT, 자동차 | 비용 낮음 |
| **Universität Stuttgart** | M.Sc. Electrical Engineering | 자동차, 임베디드, ADAS | 산업 연계 |
| **Hochschule (FH)** | M.Eng. Automotive Engineering | 실무 중심 | PhD 진입은 대학(Uni) 선호 |

### 2.2 필수 과목 (Master에서 반드시 수강)

- **Embedded Systems / Real-Time Systems** (RTOS, WCET, scheduling)
- **Automotive Software Engineering** (AUTOSAR, RTE, SWC)
- **Machine Learning on Edge Devices** (TinyML, quantization, pruning)
- **Formal Methods / Verification** (model checking, SMT, IBP)
- **Functional Safety** (ISO 26262, FMEA, FTA)
- **Hardware/Software Co-Design** (MPU, TrustZone, DMA)
- **German language** (B1–B2 이상 권장, PhD 지원 시 유리)

### 2.3 HiWi / Werkstudent (병행 필수)

- **HiWi (Hilfswissenschaftler)**: Lehrstuhl에서 연구 보조, 월 400–600 €, 연구 네트워크 진입
- **Werkstudent**: 자동차 OEM/공급사 (Bosch, Continental, ZF, Infineon, Vector, Elektrobit 등)에서 실무, 월 1,200–2,000 €
- **목표**: Master 1학년부터 HiWi, 2학년부터 Werkstudent 또는 둘 병행

### 2.4 현재 프로젝트 활용
- Master 지원 시: "I built a reproducible concept demonstrator for a C/Rust hybrid ECU" → 포트폴리오 링크
- HiWi/Werkstudent 지원 시: "I have hands-on experience with embedded Rust, MPU partitioning, and HIL fault injection" → 실무 어필

---

## Stage 3 — Masterarbeit (2029, 4–6개월)

**목표**: 현재 PoC를 **측정 가능한 연구 질문**으로 확장하여 1.0 (sehr gut) 달성 + Promotion 진입.

### 3.1 추천 Masterarbeit 주제 (현재 프로젝트 확장)

| # | 주제 | 연구 질문 | 확장 방향 |
|---|---|---|---|
| MA-1 | **WCET-Guaranteed TinyML on Cortex-M33** | Q2.13 FNN의 worst-case execution time을 형식적으로 보장할 수 있는가? | WCET 분석 도구 (aiT, SWEET) 적용, cycle-accurate 측정 |
| MA-2 | **Physical Adversarial Robustness of Dual-Hall Gradiometer** | 실제 자기 스푸핑 공격에 대한 false-positive/negative rate? | FEM 시뮬레이션 + 실제 Hall sensor 실험 |
| MA-3 | **Formal Verification of C/Rust ECU Partition** | MPU + Rust 타입 시스템으로 Freedom from Interference를 증명할 수 있는가? | CBMC/KLEE 또는 RustBelt 기반 검증 |
| MA-4 | **HIL Fidelity Analysis** | host 에뮬레이션이 실제 target 동작을 얼마나 정확히 예측하는가? | Cortex-M33 개발 보드에서 동일 시나리오 실행 + 비교 |
| MA-5 | **Synthetic-to-Real Generalization of Magnetic Compensation** | 합성 데이터 모델이 실제 센서에서 얼마나 일반화되는가? | 실측 데이터 수집 + domain adaptation |

### 3.2 Masterarbeit 실행 계획 (6개월)

```
Month 1: 문헌 조사 + 연구 질문 정교화 + 실험 환경 구축
Month 2: 실험 설계 + 데이터 수집 (실측 또는 FEM)
Month 3: 구현/측정 + 중간 결과
Month 4: 분석 + 통계 검증
Month 5: 논문 작성 (영문/독일어, 40-60 페이지)
Month 6: 리뷰/수정 + 발표 (Kolloquium)
```

### 3.3 핵심 산출물
- **Masterarbeit (thesis)**: 1.0 목표
- **재현 가능한 실험 코드**: GitHub 공개 (DOI via Zenodo)
- **선택적**: 짧은 workshop paper (DATE/ESWEEK student track)

---

## Stage 4 — Promotion (Dr.-Ing.) (2030–2034)

**목표**: 박사과정 진입 → Three-Pillar 연구 → Dr.-Ing. 취득 → TV-L E13 100% 연구직.

### 4.1 Three-Pillar 연구 구조 (박사 주제)

현재 PoC를 3개의 독립적이지만 연결된 연구 축으로 확장:

| Pillar | 주제 | 핵심 기여 | 출판 타겟 |
|---|---|---|---|
| **Pillar 1: TinyML Safety** | 형식 검증 가능한 고정소수점 NN on MCU | IBP bounds + WCET + 정확도 트레이드오프 | RTSS, EMSOFT, DATE |
| **Pillar 2: Physics-Informed AI Safety** | 물리 모델 기반 AI 안전 모니터링 | 1oo2 plausibility + ISO/IEC 27090 대응 | SAE, IEEE T-IV, IV |
| **Pillar 3: Systems Safety** | C/Rust 하이브리드 ECU의 Freedom from Interference | MPU + 타입 시스템 + 형식 검증 | EMSOFT, RTAS, SAE |

### 4.2 Promotion 진입 경로

```
Option A: Externe Promotion (외부 박사)
  - 산업계/Bosch/Continental 등에서 근무하며 외부 박사
  - Vorteil: 수입 유지, 산업 데이터 접근
  - Nachteil: 연구 시간 확보 어려움

Option B: Interne Promotion (내부 박사, Lehrstuhl)
  - Masterarbeit 우수 성적 → Promotionsstelle (TV-L E13, 100%, 3.5-4.5년)
  - Vorteil: 연구 집중, Betreuer 밀착
  - Nachteil: 경쟁 높음, 1.0 필요

Option C: Graduiertenkolleg / Research Training Group
  - DFG-funded RTG (예: "Trustworthy AI in Embedded Systems")
  - Vorteil: 구조화된 교육, 네트워크
  - Nachteil: 지원 경쟁 매우 높음
```

**권장**: Option B (내부 Promotion) — Masterarbeit에서 Lehrstuhl과 관계 구축 후 Promotionsstelle 지원.

### 4.3 박사과정 타임라인

```
Year 1 (Promotion): 문헌 조사 + 연구 질문 정교화 + 첫 번째 논문 (workshop)
Year 2: Pillar 1 핵심 실험 + conference paper (DATE/ESWEEK)
Year 3: Pillar 2/3 확장 + journal paper + 학회 발표
Year 4: 논문 작성 (Dissertation, 150-250 페이지) + Disputation
```

### 4.4 박사 학위 취득 후 진로

| 경로 | 직책 | 연봉 (독일, 2026 기준) | 비고 |
|---|---|---|---|
| 산업계 (OEM/공급사) | Senior Engineer / Lead | 85,000–120,000 € | Bosch, Continental, ZF, Infineon |
| 산업계 (Tier-2/스타트업) | Principal Engineer | 90,000–130,000 € | Vector, Elektrobit, Apex.AI |
| 학계 | Junior Professor / Akademischer Rat | 70,000–90,000 € | Habilitation 경로 |
| 연구소 | Fraunhofer/DFKI 연구원 | 75,000–100,000 € | 응용 연구 |

---

## Stage 5 — 자금/장학금 (병행)

| 장학금/지원 | 대상 | 금액 | 비고 |
|---|---|---|---|
| **DAAD** (독일 학술 교류처) | 외국인 학생/연구자 | 월 934 € (석사), 1,300 € (박사) | 경쟁 높음, 학업 성적 중요 |
| **Deutschlandstipendium** | 우수 학생 | 월 300 € | 학과 추천 |
| **Promotionsstelle (TV-L E13)** | 박사과정 | 월 ~4,800 € (brutto, 100%) | Lehrstuhl 내부 채용 |
| **Bosch/Continental Stipendium** | 특정 분야 학생 | 가변 | 산업 파트너십 |
| **Förderverein** (학과 동문) | 학생 | 소액 | 네트워킹 |

---

## Stage 6 — 실행 체크리스트 (즉시 시작 가능)

### 2026 Q4 (지금)
- [ ] GitHub 레포 공개 + README 정리 + 데모 영상
- [ ] 기술 블로그 1편 (영문)
- [ ] LinkedIn 정비
- [ ] 독일어 학습 시작 (Duolingo/Goethe, 목표 B1)
- [ ] 대학 3–5곳 리스트업 + 입학 요건 확인

### 2027 H1
- [ ] Master 지원 (영어 프로그램 또는 독일어 프로그램)
- [ ] HiWi/Werkstudent 지원 (Bosch, Continental, Vector, Infineon)
- [ ] 현재 프로젝트 v2: 실제 센서 데이터 수집 (STM32 개발 보드 + TLV493D)
- [ ] 논문 1편 (workshop track) 제출

### 2027 H2 – 2028
- [ ] Master 1학년: 필수 과목 + HiWi
- [ ] Master 2학년: Werkstudent + Masterarbeit 주제 확정
- [ ] conference paper 1편 (DATE/ESWEEK student track)

### 2029
- [ ] Masterarbeit 완료 (1.0 목표)
- [ ] Promotionsstelle 지원 (Lehrstuhl)
- [ ] Promotion 시작

### 2030–2034
- [ ] Three-Pillar 연구 실행
- [ ] conference paper 2–3편 + journal paper 1편
- [ ] Dissertation + Disputation → Dr.-Ing.

---

## 부록 A — 독일 자동차 임베디드 Lehrstuhl 추천 (2026 기준)

| Lehrstuhl | 대학 | 연구 분야 | 연락 방법 |
|---|---|---|---|
| Lehrstuhl für Echtzeitsysteme | TUM | RTOS, WCET, embedded | 웹사이트 → HiWi 공고 |
| Institut für Technische Informatik | KIT | embedded, automotive | HiWi/Werkstudent 공고 |
| Institut für Regelungstechnik | TU Braunschweig | ADAS, vehicle control | 연구 그룹 연락 |
| Chair of Embedded Software | RWTH Aachen | embedded, formal methods | HiWi 공고 |
| Lehrstuhl für Rechnerarchitektur | TU Dresden | embedded, IoT | HiWi 공고 |

## 부록 B — 출판 타겟 (임베디드/자동차)

| 학회/저널 | 분야 | 마감 (대략) | 난이도 |
|---|---|---|---|
| DATE (Design, Automation & Test in Europe) | EDA, embedded, HW/SW | 9월 | 중 |
| ESWEEK (Embedded Systems Week) | embedded, RTOS, IoT | 4월 | 중-고 |
| RTSS (Real-Time Systems Symposium) | real-time, scheduling | 5월 | 고 |
| EMSOFT | embedded software | 4월 | 고 |
| RTAS (Real-Time and Embedded Technology) | RTOS, WCET | 10월 | 중-고 |
| SAE World Congress | automotive | 8월 | 중 |
| IEEE T-IV (Transactions on Intelligent Vehicles) | 자동차 AI | 수시 | 중 |
| IEEE Embedded Systems Letters | embedded | 수시 | 중 |

## 부록 C — 현재 프로젝트 → 연구 질문 매핑

```
safe-sensor-ecu (PoC)
  ├── RQ1: WCET-guaranteed TinyML → MA-1, Pillar 1
  ├── RQ2: Physics-informed AI safety → MA-2, Pillar 2
  ├── RQ3: Formal verification of C/Rust partition → MA-3, Pillar 3
  ├── RQ4: HIL fidelity → MA-4
  └── RQ5: Synthetic-to-real generalization → MA-5
```

---

> **면책**: 본 로드맵은 2026년 기준 독일 대학원/박사과정 정보를 바탕으로 작성되었습니다.
> 입학 요건, 장학금, TV-L 급여 등은 변경될 수 있으므로 반드시 공식 출처(대학 웹사이트, DAAD, DFG)를 확인하세요.
> 기존 `docs/01-ee-master-phd-action-plan.md`, `docs/02-german-grad-phd-roadmap.md`와 함께 참고하세요.
