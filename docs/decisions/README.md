# Architecture Decision Records

되돌리기 어렵거나 여러 실험에 영향을 주는 기술 선택은 짧은 ADR로 기록한다. 사소하거나 쉽게 되돌릴 수 있는 선택까지 문서화하지 않는다.

- 파일명은 `NNNN-short-title.md` 형식을 사용한다.
- 결정 전에는 `제안` 상태로 작성하고 합의 또는 검증 후 `채택`으로 바꾼다.
- 가설을 사실처럼 표현하지 않으며, 실험 결과나 근거를 연결한다.
- 결정이 바뀌면 기존 기록을 삭제하지 않고 `대체됨` 상태와 후속 ADR을 표시한다.

## 목록

| 번호 | 제목 |
| --- | --- |
| [0001](0001-prototype-tech-stack.md) | 초기 프로토타입에 C++20 + SFML 3 + CMake를 사용한다 |
| [0002](0002-minimal-source-structure.md) | 필요해질 때까지 엔진 계층을 만들지 않고 최소 구조를 유지한다 |
| [0003](0003-movement-state-model.md) | 이동 상태를 명시적 모델로 두되 프레임워크로 키우지 않는다 |
| [0004](0004-region-based-world-structure.md) | 월드를 지역 단위로 나누고 플레이어 수명을 지역 수명과 분리한다 |
| [0005](0005-display-and-view-policy.md) | 물리 해상도와 논리 월드 뷰를 분리하고 화면비 정책을 한 곳에서 관리한다 |
| [0006](0006-upward-motion-is-jumping.md) | 위로 올라가는 동안은 원인과 무관하게 Jumping 으로 본다 |
| [0007](0007-regions-as-data.md) | 지역을 코드가 아니라 데이터 파일로 둔다 |
| [0008](0008-parallax-background-layers.md) | 배경을 지역과 분리된 데이터 파일의 시차 레이어로 둔다 |
| [0009](0009-tiles-derived-from-collision.md) | 지형 타일을 충돌 데이터에서 유도한다 |
| [0010](0010-inner-macro.md) | inner 매크로 반복 텍스처 |
| [0011](0011-terrain-boundary-assets.md) | 지형 경계는 inner 위에 얹는 오버레이 에셋으로 만든다 |
| [0012](0012-avatar-lake-decoration-layer.md) | Avatar Lake 데코레이션 레이어 |

## 템플릿

```markdown
# NNNN: 결정 제목

- 상태: 제안 | 채택 | 대체됨 | 폐기
- 날짜: YYYY-MM-DD

## 결정 내용

무엇을 선택하거나 변경하는가?

## 배경

어떤 문제와 제약 때문에 결정이 필요한가?

## 검토한 대안

- 대안과 주요 장단점

## 선택 이유

어떤 근거와 실험 결과로 이 선택을 했는가?

## 되돌릴 조건

어떤 관측이나 상황에서 결정을 재검토하거나 폐기하는가?
```
