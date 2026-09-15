# 데코레이션 에셋

충돌 없는 시각 장식이다. 결정과 원칙은 [ADR 0012](../../docs/decisions/0012-avatar-lake-decoration-layer.md) 에 있다.

## 데이터

- 리전 JSON 의 `"decorations": "<name>"` 이 `<name>.json` 을 기본 장식으로 연결한다.
- `items` 의 `position` 은 이미지 아래 가장자리 중앙의 월드 좌표이며, 1 텍셀은 1 월드 단위다. `image` 는 이 파일 기준 상대 경로다.
- `layer`: `behind` 는 지형 뒤, `above` 는 지형 앞·플레이어 뒤. 같은 레이어 안에서는 파일 순서대로 그린다.
- 실행 옵션 `--decorations <file>` 로 시험 세트를, `--decorations none` 으로 장식 없는 화면을 본다. `--tileset` 을 바꾸고 `--decorations` 를 주지 않으면 장식을 싣지 않는다.

## 폴더 규칙

지형 타일(`assets/tiles/<region>/`)과 같은 구조를 쓴다.

| 경로 | 내용 |
| --- | --- |
| `<region>/requests/` | 생성 주문서와 스케치. 상단에 역할·판정 상태 |
| `<region>/source/` | 받은 raw 원본(불채택 포함). 편집·리사이즈하지 않는다 |
| `<region>/runtime/` | **승인** 장식과 `.provenance.json`(`status: approved`). 기본 데이터는 여기만 참조한다 |
| `<region>/runtime/trials/` | 판정 대기 또는 불채택 시험 기록(`status: trial`). 기본 데이터가 참조하면 검사가 실패한다 |
| `<region>.json` | 리전 기본 장식 |
| `<region>_*_trial.json` | 시험 세트 |
