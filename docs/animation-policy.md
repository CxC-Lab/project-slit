# Project Slit 플레이어 스프라이트 및 이동 애니메이션 정책

이 문서는 Project Slit 플레이어 아바타의 2D 스프라이트 제작과 이동 애니메이션에 대한 정책을 기록하는 살아 있는 문서다. [`project-brief.md`](project-brief.md)의 "작은 실험으로 먼저 검증하고, 살아남은 결과만 확정한다"는 원칙을 따르며, 아래 내용은 대부분 검증되지 않은 가설과 후보로 남는다. 캐릭터 종수, 정확한 프레임 수, canvas 크기 등은 확정된 production requirement가 아니다.

관련 배경은 [`visual-direction.md`](visual-direction.md)의 "플레이어 아바타와 스프라이트 정책 가설" 절에도 기록되어 있다. 두 문서 사이의 차이는 해당 절 상단의 참고 문구를 확인한다.

## 1. 아바타 제작 기본 방향

Project Slit의 플레이어 아바타는 작은 SD 비율의 2D 로봇형 캐릭터를 우선 고려한다.

중요:

- SD 로봇이라는 공통 방향은 있으나, 캐릭터별 체형과 실루엣의 개성을 강하게 허용한다.
- 특정 공통 스켈레톤이나 뼈대를 공유하는 방식은 현재 기본 방향이 아니다.
- 홀쭉한 체형, 둥근 체형, 머리가 큰 체형, 다리가 긴 체형 등 서로 큰 차이를 가질 수 있다.
- 애니메이션 일관성을 위해 공유하는 것은 "공통 스켈레톤"이 아니라 "공통 제작 규격"이다.

공통 제작 규격의 예:

- frame canvas size
- pivot / anchor 규칙
- animation state 이름
- frame layout 규칙
- collider와 visual sprite의 분리

캐릭터 디자인 자체는 각각 독립적일 수 있다.

현재 다수 캐릭터를 최종적으로 몇 종 제공할지는 확정되지 않았다. 64종은 장기 후보일 뿐이며 현재 목표가 아니다.

현재 단계에서는 소수의 대표 캐릭터로 규격과 이동 애니메이션을 먼저 검증한다.

## 2. 애니메이션의 역할

Project Slit에서는 이동 자체가 핵심 재미 중 하나다.

따라서 애니메이션은 단순 장식이 아니라 플레이어에게 이동 상태, 속도, 방향 변화, 무게감, 반응성을 전달하는 핵심 요소로 본다.

"플레이에 문제가 없으면 애니메이션은 최소화해도 된다"는 방향을 채택하지 않는다.

다만 모든 동작에 많은 프레임을 사용하는 것도 목표가 아니다.

핵심 원칙:

- 프레임 수 최소화보다 이동감과 상태 전달이 우선한다.
- 필요한 상태에는 충분한 애니메이션을 제공한다.
- 중요도가 낮은 상태는 transform, offset, particle, shader 등의 보조 표현으로 보완할 수 있다.
- 캐릭터가 단순히 좌표만 이동하는 것처럼 보이지 않도록 한다.

## 3. 피해야 할 허접한 이동 표현

다음과 같은 표현은 피한다.

- Idle 상태에서 캐릭터가 완전히 정지된 한 장 이미지처럼 보이는 것
- 달리기 중 방향을 바꾸었는데 즉시 좌우 flip만 되어 무게 변화가 전혀 없는 것
- 이동 시작과 정지에 아무런 시각적 반응이 없는 것
- 점프 직전 준비 동작 없이 즉시 위로 이동하는 것
- 착지 시 반동이 없는 것
- 공중 대쉬가 단순히 좌표값만 순간적으로 변한 것처럼 보이는 것
- Run, Jump, Dash 등의 상태 차이가 실루엣상 거의 읽히지 않는 것

2D 게임이므로 3D 캐릭터의 실제 yaw/pitch 회전을 그대로 구현하는 것이 목적은 아니다.

대신 다음과 같은 2D 표현으로 방향 전환과 무게감을 전달할 수 있다.

예:

- 좌우 flip
- 짧은 turn pose
- sprite rotation
- 1~2px visual offset
- squash / stretch
- lean
- landing compression
- dash afterimage
- particle effect

이들은 모두 확정 구현 방식이 아니라 검증 가능한 표현 후보로 기록한다.

## 4. 중요 애니메이션 상태

현재 다음 상태들을 우선 중요 상태로 본다.

- Idle
- Walk
- Run
- Turn
- Jump Start
- Airborne
- Land
- Air Dash
- Hover
- Fast Fall / Dive
- Wall-related motion
- Hang
- Ground Slide
- Music

모든 상태의 정확한 frame 수는 아직 확정하지 않는다.

초기 검증용 가이드 후보는 다음과 같다.

| 상태 | 프레임 수 후보 |
| --- | ---: |
| Idle | 약 3~4 |
| Walk | 약 3~4 |
| Run | 약 4~6 |
| Turn | 약 1~2 |
| Jump Start | 약 1 |
| Airborne | 약 1~2 |
| Land | 약 1 |
| Air Dash | 약 2 |
| Hover | 약 1~2 |
| Fast Fall / Dive | 약 1~2 |
| Wall / Hang | 약 1~3 |
| Ground Slide | 약 1~2 |
| Music | 약 2 |

중요: 이 수치는 확정된 production requirement가 아니다. 실제 테스트 후 늘어나거나 줄어들 수 있다.

특히 Run, Turn, Jump Start, Land, Air Dash는 이동감을 살리는 데 중요한 상태로 취급한다.

## 5. Idle 정책

Idle은 반드시 많은 프레임을 사용할 필요는 없지만, 완전히 정적인 이미지처럼 보이지 않도록 한다.

예:

- 작은 body bob
- 미세한 head movement
- LED flicker
- breathing-like mechanical motion
- antenna movement

이 표현은 sprite frame 또는 transform 효과를 사용할 수 있다.

목적은 "멈춰 있어도 살아 있는 캐릭터"처럼 보이게 하는 것이다.

## 6. Start / Stop / Turn 정책

이동 시작, 정지, 방향 전환은 이동감에 중요한 요소다.

다음과 같은 상태 또는 표현을 검토한다.

Start:

- 달리기 시작 시 몸이 이동 방향으로 약간 기울어짐
- acceleration을 시각적으로 표현

Stop:

- 입력 종료 시 아주 짧은 관성 또는 자세 변화
- 필요 시 skid 또는 작은 particle 사용 가능

Turn:

- 반대 방향 입력 시 즉시 flip만 하지 않고 짧은 turn pose, lean 또는 compression을 사용할 수 있음

정확한 구현 방식은 프로토타입에서 검증한다.

## 7. Jump / Land 정책

점프는 다음 최소 상태를 구분하는 방향을 우선 검토한다.

- Jump Start
- Airborne
- Land

Jump Start:

- 점프 직전 압축 또는 준비 자세

Airborne:

- 공중 기본 자세

Land:

- 착지 시 짧은 compression 또는 충격 표현

점프 전체를 한 장의 sprite로 처리하는 것은 우선 방향이 아니다.

## 8. 공중 이동 표현

Project Slit은 공중 이동을 중요한 이동 요소로 고려하고 있다.

현재 검토 중인 공중 행동:

- 8방향 Air Dash
- Hover
- Fast Fall / Dive

Air Dash는 일반 Jump 또는 Run과 다른 시각적 언어를 가져야 한다.

예:

- 전용 dash pose
- sprite stretch
- lean
- afterimage
- speed particle

Hover: 별도 sprite frame 또는 idle/airborne pose + bobbing transform으로 표현 가능.

Fast Fall: 별도 pose 또는 airborne pose 변형. 아래 방향성이 명확히 보여야 함.

## 9. 스프라이트 캔버스 규칙

모든 프레임은 동일한 고정 canvas size를 사용하는 방향을 우선 검토한다.

중요:

- 프레임마다 실제 character bounding box에 맞춰 crop하지 않는다.
- 프레임마다 이미지 크기를 바꾸지 않는다.
- 투명 여백을 허용한다.
- 캐릭터 동작에 따라 손, 머리, 장식물 등의 bounding box 크기가 달라도 canvas는 동일하게 유지한다.

정확한 frame size는 아직 확정하지 않는다.

예시 후보:

- 64x64
- 96x96
- 128x128

이 수치는 문서상 후보로만 남긴다.

## 10. Pivot / Anchor 정책

애니메이션 재생 중 캐릭터가 프레임마다 흔들리는 현상을 방지하기 위해 고정 pivot / anchor 규칙을 사용한다.

기본 방향:

- 발 위치를 공통 기준점으로 사용
- horizontal center 또는 캐릭터별 정의된 기준 X 사용
- 모든 frame에서 동일한 기준점 유지

중요:

- frame bounding box center를 기준으로 정렬하지 않는다.
- 캐릭터의 월드 위치는 animation frame에 따라 변하지 않는다.

매달리기나 벽 동작처럼 손 위치가 중요하더라도 기본 world pivot을 바꾸기보다는 visual offset을 별도로 사용하는 방향을 우선 검토한다.

## 11. Visual Offset 정책

필요한 경우 animation 또는 frame 단위 visual offset을 허용한다.

예: `visualOffset = (+2, -1)`

visual offset의 목적:

- sprite 제작 과정에서 발생하는 미세한 정렬 오차 보정
- wall, hang, slide 등의 시각적 접촉 위치 보정

visual offset은 다음에 영향을 주지 않는다.

- world position
- velocity
- physics collider
- gameplay state

즉 rendering-only 보정값으로 취급한다.

## 12. Sprite와 Physics 분리

sprite image의 실제 크기와 player physics는 독립적으로 관리한다.

예:

- 장식물이 크게 뻗어도 collider가 커지지 않음
- jump frame에서 몸을 웅크려도 collider가 자동 축소되지 않음
- slide 등 collider 변경이 필요한 동작은 animation이 아니라 gameplay state가 명시적으로 결정

다음 요소를 분리해서 관리하는 방향을 기록한다.

- world position
- velocity
- collider
- sprite
- animation state
- visual offset
- visual transform

## 13. Transform 기반 보조 표현

스프라이트 프레임만으로 모든 표현을 해결할 필요는 없다.

다음과 같은 rendering transform을 애니메이션 보조 수단으로 사용할 수 있다.

- sprite rotation
- X/Y scale
- squash / stretch
- lean
- small bobbing
- visual offset
- flip
- afterimage
- particle
- shader

목적:

- 프레임 제작량을 무조건 줄이기 위함이 아니다.
- 필요한 이동감을 더 적은 제작 비용으로 강화하기 위함이다.

중요: 필요한 sprite animation을 transform으로 전부 대체한다는 의미가 아니다.

## 14. Music 애니메이션

악기 종류별로 캐릭터 애니메이션을 개별 제작하지 않는 방향을 우선 검토한다.

캐릭터는 공통 Music animation을 사용한다.

예:

- Music Idle
- Music Play

악기별 차이는 캐릭터가 직접 기타, 피아노, 피리 등을 들고 각각 다른 자세를 취하는 방식보다 공통된 만능 연주 장치 또는 캐릭터 앞에 펼쳐지는 장치 등을 우선 고려한다.

악기 차이는 다음 요소로 표현 가능하다.

- MIDI instrument / patch
- SoundFont
- timbre
- small visual effect
- device visual variation

목적: avatar count × instrument count 만큼 애니메이션 제작량이 증가하는 것을 방지한다.

## 15. 아바타 수 정책

다수의 캐릭터를 제공하는 방향은 장기적으로 고려하고 있으나, 현재는 64종 제작을 시작하지 않는다.

현재 우선순위:

1. 대표 캐릭터 1종
2. 체형이 크게 다른 대표 캐릭터 추가
3. 동일 animation state 체계가 서로 다른 실루엣에서도 성립하는지 검증
4. pivot / canvas / transform 정책 검증
5. 이후 캐릭터 수 확대 여부 결정

초기 테스트 캐릭터는 서로 체형 차이가 큰 것이 좋다.

예:

- 가늘고 긴 체형
- 둥글고 짧은 체형

이 예시는 실제 최종 캐릭터 디자인을 의미하지 않는다.

## 16. 초기 애니메이션 검증 순서

다음 순서를 문서에 반영한다.

1. 임시 geometry 또는 placeholder로 이동 기능 검증
2. 테스트 캐릭터 1종 적용
3. Idle
4. Walk / Run
5. Start / Stop / Turn
6. Jump Start / Airborne / Land
7. Air Dash
8. Hover / Fast Fall
9. Wall / Hang / Slide
10. Music
11. 체형이 크게 다른 두 번째 캐릭터에 동일 규격 적용
12. 두 캐릭터 모두에서 pivot 흔들림과 이동감 검증
13. 규격이 안정된 뒤에만 캐릭터 수 확대

## 17. 품질 검증 기준

애니메이션을 적용한 뒤 최소 다음을 육안으로 검증한다.

- Idle이 정적 이미지처럼 보이지 않는가
- Run에서 충분한 속도감이 느껴지는가
- 방향 전환이 단순 flip처럼 보이지 않는가
- Jump Start가 점프 의도를 전달하는가
- Land가 착지 충격을 전달하는가
- Air Dash가 일반 공중 이동과 명확히 구분되는가
- frame 변경 시 발 위치가 흔들리지 않는가
- 서로 다른 체형에서도 animation state가 자연스럽게 읽히는가
- sprite와 collider가 독립적으로 동작하는가

이 항목들은 automated test만으로 충분하지 않으며 실제 플레이 화면을 통한 visual playtest가 필요하다는 점을 기록한다.
