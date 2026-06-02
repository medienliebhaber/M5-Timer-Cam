from app.storage.repository import FrameRepository


def test_insert_and_get(repo):
    frame = repo.insert(
        captured_at="2026-06-02T14:00:00Z",
        trigger="timer",
        filename="2026/06/02/14-00-00_timer.jpg",
        filesize=12345,
        width=1600,
        height=1200,
    )
    assert frame.id > 0
    assert frame.captured_at == "2026-06-02T14:00:00Z"
    assert frame.trigger == "timer"
    assert frame.width == 1600

    fetched = repo.get_by_id(frame.id)
    assert fetched == frame


def test_list_pagination(repo):
    for i in range(5):
        repo.insert(
            captured_at=f"2026-06-02T{i:02d}:00:00Z",
            trigger="timer",
            filename=f"2026/06/02/{i:02d}-00-00_timer.jpg",
        )

    frames, total = repo.list_frames(page=1, per_page=3)
    assert total == 5
    assert len(frames) == 3

    frames2, _ = repo.list_frames(page=2, per_page=3)
    assert len(frames2) == 2


def test_list_date_filter(repo):
    repo.insert("2026-06-01T12:00:00Z", "timer", "a.jpg")
    repo.insert("2026-06-02T12:00:00Z", "timer", "b.jpg")
    repo.insert("2026-06-03T12:00:00Z", "timer", "c.jpg")

    frames, total = repo.list_frames(from_dt="2026-06-02T00:00:00Z")
    assert total == 2


def test_get_nonexistent(repo):
    assert repo.get_by_id(9999) is None
