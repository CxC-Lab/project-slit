# 0010: inner 매크로 반복 텍스처

- 상태: 채택 (inner 역할 한정. 나머지 9개 역할은 진단 표현)
- 날짜: 2026-09-13

## 근거와 시험 범위

20x20 절차적 정규화의 반복이 바코드처럼 보였다는 실험 기록에 따라, 원본의 큰 암반 형태를
보존한 후보를 별도 반복 텍스처로 시험한다. E1_medium은 SEAM_FAIL로 승격하지 않는다.
승인된 런타임 후보는 전체 원본을 120x120으로 줄인 E0_full_120이다.
80x80 이미지를 20x20 아틀라스 칸에 넣거나 축소하지 않는다. 기존 진단 아틀라스는 그대로 둔다.
근거는 `assets/tiles/avatar_lake/candidates/desktop-2026-09-13/records/scale_experiment_report.md`와
같은 디렉터리의 `inner_macro_architecture_review.md`다.

## 데이터와 코드의 경계

기존 타일셋 JSON에 선택적으로 다음 설정을 덧붙인다.

```json
"sampling": {
  "inner": {"mode": "repeat2d", "image": "../tiles/avatar_lake/runtime/avatar_lake_inner_e0_full_120.png"}
}
```

설정이 없으면 열 역할 모두 기존 아틀라스를 쓴다. 현재는 inner/repeat2d만 허용하며 다른 역할이나
모드는 명확히 거부한다. 역할 판정은 기존 충돌 합집합 계산 그대로이고, 판정 이후 데이터로 선택한
샘플링 방식에 따라 UV와 렌더 배치를 고른다. 역할 번호나 이미지별 UV 오프셋은 추가하지 않는다.

현재 판정은 다음과 같다.

| 후보 | 크기 | 판정 |
| --- | --- | --- |
| E0_full_120 | 원본 전체 → 120x120 | 승인. 런타임 후보로 내보냄 |
| E1_medium | 원본 75% 크롭 → 80x80 | SEAM_FAIL. 승격하지 않음 |
| E0_current | 원본 전체 → 80x80 | 비교·복귀 후보. 이음선은 안정적이나 암반이 지나치게 촘촘함 |
| E0_full_160 | 원본 전체 → 160x160 | 비교·복귀 후보. 무늬와 픽셀이 커 반복 덩어리가 쉽게 인식됨 |

`--tileset <name>`은 비교와 문제 해결 용도다. 명시한 실행에서만 Region 생성 시 타일셋 이름을
대체하며, 충돌 좌표, 지역 id/seed, 스폰, 배경은 원래 지역 것을 쓴다.

## 좌표와 렌더링

- UV = 그려지는 월드 좌표 - 연결 덩어리의 `bounds.position`.
- 음수 원점에서도 같은 뺄셈을 쓴다. 사각형·셀·카메라를 기준으로 다시 시작하지 않는다.
- 맞닿는 사각형은 같은 덩어리 원점을 공유한다. 별개의 덩어리는 각자 원점을 쓴다.
- 조각의 UV 크기와 월드 크기는 동일하다. 반복 주기는 실제 이미지 크기에서 나온다.
- `setRepeated(true)`, `setSmooth(false)`로 1 텍셀 = 1 월드 단위를 유지한다.
- 10-role 아틀라스 크기 검증은 유지한다. 나머지 9개 역할의 변형 해시와 경계 UV도 유지한다.
- TerrainTiles 인스턴스는 매크로 텍스처를 최초 render 때 한 번 로드하고 모든 셀·덩어리가 공유한다.
  아틀라스와 매크로를 각각 한 배치로 그린다. 파일 실패는 경로를 포함한 예외로 보고한다.
- 이번에는 매크로 파일 하나뿐이므로 변형 선택은 없다. 향후 여러 파일을 허용할 때는 덩어리 단위로
  결정해야 하며 셀 해시로 서로 다른 매크로를 섞지 않는다. 전역 리소스 캐시는 도입하지 않는다.

## 원본과 생성물

`source/` 원본 → `avatar_lake.pipeline.manifest.json`의 크롭·팔레트 설정 → 오프라인 도구의
build 결과 → 검증·사용자 승인 후 런타임 자산이라는 경계를 유지한다. 런타임은 source와 build
프리뷰를 읽지 않는다. 런타임 후보는 `assets/tiles/avatar_lake/runtime/`에 두며,
`avatar_lake_inner_e0_full_120.provenance.json`이 후보 id, 매니페스트, 원본 및 런타임 SHA-256,
크롭·출력·단계·상태를 기록한다. `rock_inner_01/02.png`와 기존 아틀라스는 덮어쓰지 않는다.
새 원본 또는 반복 주기는 재생성 후 시험 타일셋의 image 경로로 선택하며 렌더링 코드를 바꾸지 않는다.

## 미래 확장: 미구현

표현 범주는 2차원 반복(inner), 1축 반복 가능성(top/bottom/left/right), 고정 또는 조합
(모서리 네 개와 slab)으로 나눌 수 있다. 새 역할을 구현한다면 샘플링 방식과 반복 축을 데이터로
선언하고 공통 좌표 변환을 확장해야 한다. 역할별 수동 숫자나 전용 UV 분기를 나열하지 않는다.
현재 repeat2d를 경계 역할에 그대로 적용하는 것은 허용하지 않는다.

### 알려진 구조 한계 (2026-09-13 리뷰 기록)

샘플링 방식은 데이터로 선언하지만, 그 방식이 참조하는 이미지와 텍스처는 아직 역할별 목록이 아니다.
`Terrain::Tileset::innerImage` 와 `TerrainTiles::innerTexture_` 가 inner 전용 이름의 단일 필드이고,
파서가 `role != "inner"` 를 문자열로 거부한다. 이 상태로 top 에 1축 반복을 붙이면 이미지 필드,
텍스처 멤버, 파서 허용 목록이 역할마다 하나씩 복제된다. 이번 시험 범위(inner 하나)에서는 동작상
문제가 없어 보류했지만, 두 번째 역할을 받기 전에 반드시 고친다.

최소 수정 방향은 다음과 같다. 동작은 바뀌지 않는다.

- 이미지 경로와 텍스처를 역할 인덱스로 인덱싱되는 목록으로 둔다. 기존 `sampling` 배열과 같은 모양이다.
- 파서는 역할 이름이 아니라 모드를 검사한다. 지원하지 않는 모드와, 그 역할에 아직 계약이 없는
  모드 조합만 거부한다.
- 렌더는 샘플링 방식과 텍스처별로 배치를 나눈다. 역할 이름을 코드에서 비교하지 않는다.

경계 정렬과 비반복 축의 잘라내기 계약은 별도 검증 대상이다. 새 지역은 그 지역의 타일셋 데이터만으로
현재 inner 경로를 쓴다.

## 한계와 되돌릴 조건

E1 자체 반복 이음선은 채널 평균 절대차 h≈21.05, v≈22.23으로 E0(h≈6.75, v≈7.12)보다 크다.
이음선을 보간·흐림·장식으로 숨기지 않는다.

실제 렌더 화면에서도 같은 경향이 나왔다. preview 로 카메라 (8400,375), R12-C11 을 E1 과 E0 시험 상태로
각각 그려, inner 픽셀에서 80 주기 경계를 사이에 둔 인접 픽셀 차이를 경계가 아닌 곳의 차이와 비교했다.

| 후보 | 가로 이음선 (경계 / 비경계) | 세로 이음선 (경계 / 비경계) |
| --- | --- | --- |
| E1 | 22.21 / 6.13 = 3.62배 | 21.18 / 12.55 = 1.69배 |
| E0 | 7.09 / 8.14 = 0.87배 | 6.48 / 16.60 = 0.39배 |

매핑과 주기가 같은데 E1 에서만 경계가 튄다. 좌표나 샘플링 결함이 아니라 E1 크롭이 이음매 없이
반복되도록 만들어지지 않은 결과다. 특히 가로 이음선이 두드러진다. 확대 캡처에서는 인지되며,
실제 이동 중 체감은 사용자 플레이로 판정한다. 주기는 이미지 크기이며 경계는 덩어리 원점의 정수
주기 배수다. 진단 경계 역할과 암반의 아트 연결 역시 이번 시험에서 해결하지 않는다.

카메라 이동 시 미끄러짐, 사각형 접합부 위상 단절, 다른 역할 회귀, 지나친 반복 인식이 확인되면
시험을 중단한다. 진단 화면은 `tools/run_avatar_lake_debug.bat`로 즉시 볼 수 있고, 승인 상태를
되돌리려면 `assets/tilesets/avatar_lake.json`의 `sampling`을 지우면 된다.
E0 시험 타일셋으로 같은 장소를 비교해 좌표 결함과 원본의 반복 이음선을 구분한다.
inner 후보 채택은 사용자 결정으로 확정했다.

## 승격 준비 절차와 캡처 계약

매니페스트 후보의 `approved_runtime_candidate: true`와 `runtime_file`이 내보내기 승인 데이터다.
`tools/export_tile.cmake`는 매니페스트를 새 build 경로에서 재생성하고 입력 PNG와 디코딩한 RGBA를
전수 비교한 후 내보낸다. 원본 SHA-256을 재생성 전후에 확인한다. 후보 이름에 따른 예외는 없다.
출력은 매니페스트 옆 `runtime/` 아래에만 허용하고, 기존 PNG와 기록이 다르면 어느 것도 덮어쓰지 않는다.
같은 내용은 재사용하며 새 파일은 배타적 생성으로 쓴다. 생성 도구의 원본·기준본 출력 금지는 유지한다.

저장소 루트에서 Windows CMake로 실행한다 (`cmake.exe`는 VS2022 번들 3.28 이상).

```text
cmake.exe --build build --config Release --target tile_pipeline
cmake.exe -DMANIFEST=<repo>/assets/tiles/avatar_lake/avatar_lake.pipeline.manifest.json -DCANDIDATE=E0_full_120 -DINPUT=<repo>/build/tile_pipeline/avatar_lake_full_scale_20260913/E0_full_120.png -P tools/export_tile.cmake
cmake.exe -DRECORD=<repo>/assets/tiles/avatar_lake/runtime/avatar_lake_inner_e0_full_120.provenance.json -P tools/export_tile.cmake
```

`<repo>`는 현재 저장소의 절대 경로다. 코드에 개발 PC 경로를 넣지 않는다. 기록은 전체 매니페스트
스냅샷, 저장소 상대 매니페스트 경로, 후보 id, 원본/출력 SHA-256, 런타임 상대 경로, 실제 팔레트를 담는다.
복원은 현재 매니페스트 대신 기록 속 스냅샷으로 재생성하고 출력 해시까지 검사한다. 원본과 참조 PNG는
불변 입력으로 함께 보존해야 한다. 원본이 바뀌면 복원을 거부한다.

E0_current(80)과 E0_full_160은 승인 플래그를 주거나 내보내지 않는다. 80은 보존 기준본을 읽는
`--tileset avatar_lake_inner_trial_e0`로 비교한다. 160은 `--tileset avatar_lake_inner_trial_e0_160`으로
비교한다. 160의 build 출력이 없어졌다면 파이프라인을 보존된 매니페스트로 **새 출력 폴더**에 재실행하고
160 시험 JSON의 image 경로만 그 결과로 바꾼다. 이전 매니페스트는 provenance의 `manifest`로 복구할 수
있으며 상대 입력 경로는 원래 매니페스트 디렉터리 기준이다. 기존 출력·기준본은 덮어쓰지 않는다.

승인 직후에는 기본 타일셋을 바꾸지 않고 시험 타일셋만 내보낸 런타임 파일을 가리키게 했다.
이후 실행 진입점 정리에서 기본 개발 타일셋에 연결했다. 아래 절을 따른다.

F12 캡처 JSON과 preview의 출력 `manifest.json`은 기존 필드를 유지하며 `tileset`, `tilesetMode`
(`region_default`/`override`), `innerMacroImage`, `innerMacroHash`, `innerMacroHashAlgorithm`을 더한다.
Region의 공통 함수가 실제 선택값을 제공한다. 캡처의 FNV-1a 64 해시는 파일 식별용이며 내보내기의
SHA-256과 구분한다. 명시 좌표 preview도 요청/실제 카메라 중심과 visibleRect를 cells에 기록한다.

`tools/inner_macro_capture.json`의 region, baseline, points, cameraPair, trials(id/tileset)가 촬영 계획이다.
`cmake.exe -P tools/inner_macro_capture.cmake`는 새 폴더에 모든 후보를 촬영하고 같은 계획으로
`terrain_tiles_tests --macro-captures <plan> <output>`을 실행한다. 각 후보의 inner RGBA, 나머지 역할,
카메라 이동 중 같은 월드 픽셀, 기록의 실제 타일셋을 비교한다. 다른 계획은 `-DCONFIG=<file>`로 준다.
후보 추가는 이 데이터만 바꾼다. 단위 테스트는 합성 타일셋을 쓰고 CTest는 build의 임시 계획에 후보를
추가해 촬영과 검증 모두 발견하는지 확인한다. 출력 폴더가 이미 있으면 캡처 스크립트는 실패한다.

`innerImage`/`innerTexture_`는 이번에는 inner 하나만 지원하는 제한된 구조로 남긴다. 두 번째
실제 반복 역할을 추가하기 전에 이미지·텍스처를 역할 인덱스 목록으로 일반화하고 모드 중심
파서를 도입해야 한다.

## 실행 진입점과 기본 개발 타일셋 (2026-09-13)

후보가 생길 때마다 후보 이름이 들어간 실행 스크립트를 늘리지 않는다. 역할이 고정된 두 진입점만 둔다.

| 실행 | 보이는 것 | 타일셋 |
| --- | --- | --- |
| `tools/run_avatar_lake.bat` | 현재 승인된 Avatar Lake 개발 화면 | 지역 기본 `avatar_lake` |
| `tools/run_avatar_lake_debug.bat` | 역할별 색상 진단 아틀라스 | `--tileset avatar_lake_debug` |

타일셋 이름 `avatar_lake`의 의미를 "현재 승인된 Avatar Lake 개발 타일셋"으로 고정했다. 진단 아틀라스
설정은 `avatar_lake_debug`로 옮겼다. `assets/regions/avatar_lake.json`은 계속 `avatar_lake`를 가리키므로
지역 파일은 바꾸지 않았다. `run_avatar_lake.bat`의 실행 명령(`project_slit.exe avatar_lake`)도 그대로이며
무엇을 실행하는지 알리는 안내 한 줄만 더했다. 진단 화면용 `run_avatar_lake_debug.bat`를 새로 두었고,
맵 작업대 `preview_avatar_lake.bat`에는 진단 타일셋 지정을 더했다. 앞으로 top이나 경계 역할이 승인되면
`avatar_lake.json`의 데이터만 바뀌고, 사용자는 같은 `run_avatar_lake.bat`를 실행한다.

지역 파일이 `avatar_lake_dev` 같은 별도 이름을 가리키게 하는 방법도 검토했다. 그러면 최종 아트가 완성된
시점에 지역 기본값을 한 번 더 바꿔야 하고, 옵션 없이 실행한 게임과 개발 화면이 다른 모습이 된다.
지역이 기본으로 보여 주는 것이 곧 현재 최선의 승인 상태이고 진단은 명시적인 디버그 모드라는 쪽이
최종 상태까지 이름을 바꿀 일이 없어 택했다. EXE의 기본 지역(`practice_room`)은 바꾸지 않았고,
타일셋이 없는 지역은 영향을 받지 않는다.

사용자 맵 설계 작업대인 `tools/preview_avatar_lake.bat`는 진단 타일셋을 명시한다. 면 방향 색이 지형
배치 판단에 쓰이기 때문이다.

기본 개발 화면은 중간 상태다. inner만 E0_full_120 실제 텍스처이고 나머지 9개 역할은 진단 색이다.
후보별 `--tileset avatar_lake_inner_trial_*` 실행은 비교와 문제 해결 용도로 남긴다.

기본 타일셋이 가리키는 이미지는 build/ 를 가리키면 안 되고, inner 매크로는 매니페스트에서
`approved_runtime_candidate`로 표시된 `runtime_file`이며 provenance의 출력 해시와 일치해야 한다.
이 조건은 후보 이름이 아니라 매니페스트와 provenance를 읽어 검사한다.

원본, 매니페스트, 런타임 에셋, provenance, 타일셋의 관계는 다음과 같다.

```text
assets/tiles/avatar_lake/source/rock_raw_01.png                     불변 원본
  -> assets/tiles/avatar_lake/avatar_lake.pipeline.manifest.json     크롭, 출력 크기, 팔레트, 단계, 후보, 승인
  -> build/tile_pipeline/...                                         재생성 결과와 프리뷰 (지워져도 재생성)
  -> assets/tiles/avatar_lake/runtime/avatar_lake_inner_e0_full_120.png         승인 런타임 에셋
     assets/tiles/avatar_lake/runtime/avatar_lake_inner_e0_full_120.provenance.json  출처와 해시
  -> assets/tilesets/avatar_lake.json                                기본 개발 타일셋이 참조
```

## 다음 미술 단계 전에 확정할 것

- **`innerImage`/`innerTexture_` 일반화는 두 번째 실제 역할을 적용하기 전 필수다.** 위 "알려진 구조 한계"의
  최소 수정 방향을 따른다.
- **경계 에셋 계약을 먼저 확정한다.** top, bottom, left, right와 모서리, slab을 어떤 원본에서 어떤 크기와
  반복 축으로 만들지, inner 매크로의 120 주기와 경계 타일이 어떻게 맞닿을지, 경계 아트가 충돌 경계에
  붙는 규칙(ADR 0009)과 잘라내기 규칙을 정하기 전에는 top 이미지를 만들지 않는다.

2026-09-14: 두 항목 모두 [`0011-terrain-boundary-assets.md`](0011-terrain-boundary-assets.md) 에서 정했다. 경계 계약을 채택했고, 일반화의 데이터 구조와 최소 구현 범위를 기록했다. 구현은 아직 하지 않았다.

