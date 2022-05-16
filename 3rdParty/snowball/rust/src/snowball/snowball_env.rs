use std::borrow::Cow;
use snowball::Among;

#[derive(Debug, Clone)]
pub struct SnowballEnv<'a> {
    pub current: Cow<'a, str>,
    pub cursor: i32,
    pub limit: i32,
    pub limit_backward: i32,
    pub bra: i32,
    pub ket: i32,
}


impl<'a> SnowballEnv<'a> {
    pub fn create(value: &'a str) -> Self {
        let len = value.len();
        SnowballEnv {
            current: Cow::from(value),
            cursor: 0,
            limit: len as i32,
            limit_backward: 0,
            bra: 0,
            ket: len as i32,
        }
    }

    pub fn get_current(self) -> Cow<'a, str> {
        self.current
    }

    pub fn set_current(&mut self, current: &'a str) {
        self.current = Cow::from(current);
    }

    pub fn set_current_s(&mut self, current: String) {
        self.current = Cow::from(current);
    }

    fn replace_s(&mut self, bra: i32, ket: i32, s: &str) -> i32 {
        let adjustment = s.len() as i32 - (ket - bra);
        let mut result = String::with_capacity(self.current.len());
        {
            let (lhs, _) = self.current.split_at(bra as usize);
            let (_, rhs) = self.current.split_at(ket as usize);
            result.push_str(lhs);
            result.push_str(s);
            result.push_str(rhs);
        }
        // ... not very nice...
        let new_lim = self.limit + adjustment;
        self.limit = new_lim;
        if self.cursor >= ket {
            let new_cur = self.cursor + adjustment;
            self.cursor = new_cur;
        } else if self.cursor > bra {
            self.cursor = bra
        }
        self.current = Cow::from(result);
        adjustment
    }

    /// Check if s is after cursor.
    /// If so, move cursor to the end of s
    pub fn eq_s(&mut self, s: &str) -> bool {
        if self.cursor >= self.limit {
            return false;
        }
        if self.current[(self.cursor as usize)..].starts_with(s) {
            self.cursor += s.len() as i32;
            while !self.current.is_char_boundary(self.cursor as usize) {
                self.cursor += 1;
            }
            true
        } else {
            false
        }
    }

    /// Check if 's' is before cursor
    /// If so, move cursor to the beginning of s
    pub fn eq_s_b(&mut self, s: &str) -> bool {
        if (self.cursor - self.limit_backward) < s.len() as i32 {
            false
            // Check if cursor -s.len is a char boundary. if not well... return false obv
        } else if !self.current.is_char_boundary(self.cursor as usize - s.len()) ||
                  !self.current[self.cursor as usize - s.len()..].starts_with(s) {
            false
        } else {
            self.cursor -= s.len() as i32;
            true
        }
    }

    /// Replace string between `bra` and `ket` with s
    pub fn slice_from(&mut self, s: &str) -> bool {
        let (bra, ket) = (self.bra, self.ket);
        self.replace_s(bra, ket, s);
        true
    }

    /// Move cursor to next character
    pub fn next_char(&mut self) {
        self.cursor += 1;
        while !self.current.is_char_boundary(self.cursor as usize) {
            self.cursor += 1;
        }
    }

    /// Move cursor to previous character
    pub fn previous_char(&mut self) {
        self.cursor -= 1;
        while !self.current.is_char_boundary(self.cursor as usize) {
            self.cursor -= 1;
        }
    }

    pub fn hop(&mut self, mut delta: i32) -> bool {
        let mut res = self.cursor;
        while delta > 0 {
            delta -= 1;
            if res >= self.limit {
                return false;
            }
            res += 1;
            while res < self.limit && !self.current.is_char_boundary(res as usize) {
                res += 1;
            }
        }
        self.cursor = res;
        return true;
    }

    pub fn hop_checked(&mut self, delta: i32) -> bool {
        return delta >= 0 && self.hop(delta);
    }

    pub fn hop_back(&mut self, mut delta: i32) -> bool {
        let mut res = self.cursor;
        while delta > 0 {
            delta -= 1;
            if res <= self.limit_backward {
                return false;
            }
            res -= 1;
            while res > self.limit_backward && !self.current.is_char_boundary(res as usize) {
                res -= 1;
            }
        }
        self.cursor = res;
        return true;
    }

    pub fn hop_back_checked(&mut self, delta: i32) -> bool {
        return delta >= 0 && self.hop_back(delta);
    }

    // A grouping is represented by a minimum code point, a maximum code point,
    // and a bitfield of which code points in that range are in the grouping.
    // For example, in english.sbl, valid_LI is 'cdeghkmnrt'.
    // The minimum and maximum code points are 99 and 116,
    // so every time one of these grouping functions is called for g_valid_LI,
    // min must be 99 and max must be 116. There are 18 code points within that
    // range (inclusive) so the grouping is represented with 18 bits, plus 6 bits of padding:
    //
    // cdefghij klmnopqr st
    // 11101100 10110001 01000000
    //
    // The first bit is the least significant.
    // Those three bytes become &[0b00110111, 0b10001101, 0b00000010],
    // which is &[55, 141, 2], which is how g_valid_LI is defined in english.rs.
    /// Check if the char the cursor points to is in the grouping
    pub fn in_grouping(&mut self, chars: &[u8], min: u32, max: u32) -> bool {
        if self.cursor >= self.limit {
            return false;
        }
        if let Some(chr) = self.current[self.cursor as usize..].chars().next() {
            let mut ch = chr as u32; //codepoint as integer
            if ch > max || ch < min {
                return false;
            }
            ch -= min;
            if (chars[(ch >> 3) as usize] & (0x1 << (ch & 0x7))) == 0 {
                return false;
            }
            self.next_char();
            return true;
        }
        return false;
    }

    pub fn in_grouping_b(&mut self, chars: &[u8], min: u32, max: u32) -> bool {
        if self.cursor <= self.limit_backward {
            return false;
        }
        self.previous_char();
        if let Some(chr) = self.current[self.cursor as usize..].chars().next() {
            let mut ch = chr as u32; //codepoint as integer
            self.next_char();
            if ch > max || ch < min {
                return false;
            }
            ch -= min;
            if (chars[(ch >> 3) as usize] & (0x1 << (ch & 0x7))) == 0 {
                return false;
            }
            self.previous_char();
            return true;
        }
        return false;
    }

    pub fn out_grouping(&mut self, chars: &[u8], min: u32, max: u32) -> bool {
        if self.cursor >= self.limit {
            return false;
        }
        if let Some(chr) = self.current[self.cursor as usize..].chars().next() {
            let mut ch = chr as u32; //codepoint as integer
            if ch > max || ch < min {
                self.next_char();
                return true;
            }
            ch -= min;
            if (chars[(ch >> 3) as usize] & (0x1 << (ch & 0x7))) == 0 {
                self.next_char();
                return true;
            }
        }
        return false;
    }

    pub fn out_grouping_b(&mut self, chars: &[u8], min: u32, max: u32) -> bool {
        if self.cursor <= self.limit_backward {
            return false;
        }
        self.previous_char();
        if let Some(chr) = self.current[self.cursor as usize..].chars().next() {
            let mut ch = chr as u32; //codepoint as integer
            self.next_char();
            if ch > max || ch < min {
                self.previous_char();
                return true;
            }
            ch -= min;
            if (chars[(ch >> 3) as usize] & (0x1 << (ch & 0x7))) == 0 {
                self.previous_char();
                return true;
            }
        }
        return false;

    }


    /// Helper function that removes the string slice between `bra` and `ket`
    pub fn slice_del(&mut self) -> bool {
        self.slice_from("")
    }

    pub fn insert(&mut self, bra: i32, ket: i32, s: &str) {
        let adjustment = self.replace_s(bra, ket, s);
        if bra <= self.bra {
            self.bra = self.bra + adjustment;
        }
        if bra <= self.ket {
            self.ket = self.ket + adjustment;
        }
    }

    pub fn assign_to(&mut self) -> String {
        self.current[0..self.limit as usize].to_string()
    }

    pub fn slice_to(&mut self) -> String {
        self.current[self.bra as usize..self.ket as usize].to_string()
    }

    pub fn find_among<T>(&mut self, amongs: &[Among<T>], context: &mut T) -> i32 {
        use std::cmp::min;
        let mut i: i32 = 0;
        let mut j: i32 = amongs.len() as i32;

        let c = self.cursor;
        let l = self.limit;

        let mut common_i = 0i32;
        let mut common_j = 0i32;

        let mut first_key_inspected = false;
        loop {
            let k = i + ((j - i) >> 1);
            let mut diff: i32 = 0;
            let mut common = min(common_i, common_j);
            let w = &amongs[k as usize];
            for lvar in common..w.0.len() as i32 {
                if c + common == l {
                    diff = -1;
                    break;
                }
                diff = self.current.as_bytes()[(c + common) as usize] as i32 - w.0.as_bytes()[lvar as usize] as i32;
                if diff != 0 {
                    break;
                }
                common += 1;
            }
            if diff < 0 {
                j = k;
                common_j = common;
            } else {
                i = k;
                common_i = common;
            }
            if j - i <= 1 {
                if i > 0 {
                    break;
                }
                if j == i {
                    break;
                }
                if first_key_inspected {
                    break;
                }
                first_key_inspected = true;
            }
        }

        loop {
            let w = &amongs[i as usize];
            if common_i >= w.0.len() as i32{
                self.cursor = c + w.0.len() as i32;
                if let Some(ref method) = w.3 {
                    let res = method(self, context);
                    self.cursor = c + w.0.len() as i32;
                    if res {
                        return w.2;
                    }
                } else {
                    return w.2;
                }
            }
            i = w.1;
            if i < 0 {
                return 0;
            }
        }
    }

    pub fn find_among_b<T>(&mut self, amongs: &[Among<T>], context: &mut T) -> i32 {
        let mut i: i32 = 0;
        let mut j: i32 = amongs.len() as i32;

        let c = self.cursor;
        let lb = self.limit_backward;

        let mut common_i = 0i32;
        let mut common_j = 0i32;

        let mut first_key_inspected = false;

        loop {
            let k = i + ((j - i) >> 1);
            let mut diff: i32 = 0;
            let mut common = if common_i < common_j {
                common_i
            } else {
                common_j
            };
            let w = &amongs[k as usize];
            for lvar in (0..w.0.len() - common as usize).rev() {
                if c - common == lb {
                    diff = -1;
                    break;
                }
                diff = self.current.as_bytes()[(c - common - 1) as usize] as i32 - w.0.as_bytes()[lvar] as i32;
                if diff != 0 {
                    break;
                }
                // Count up commons. But not one character but the byte width of that char
                common += 1;
            }
            if diff < 0 {
                j = k;
                common_j = common;
            } else {
                i = k;
                common_i = common;
            }
            if j - i <= 1 {
                if i > 0 {
                    break;
                }
                if j == i {
                    break;
                }
                if first_key_inspected {
                    break;
                }
                first_key_inspected = true;
            }
        }
        loop {
            let w = &amongs[i as usize];
            if common_i >= w.0.len() as i32 {
                self.cursor = c - w.0.len() as i32;
                if let Some(ref method) = w.3 {
                    let res = method(self, context);
                    self.cursor = c - w.0.len() as i32;
                    if res {
                        return w.2;
                    }
                } else {
                    return w.2;
                }
            }
            i = w.1;
            if i < 0 {
                return 0;
            }
        }
    }
}
