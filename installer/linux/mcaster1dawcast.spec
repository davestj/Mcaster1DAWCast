Name:           mcaster1dawcast
Version:        1.0.0
Release:        0.1.alpha%{?dist}
Summary:        Multi-Channel DAW for Broadcasting, Webcasting, Podcasting & Video Editing
License:        GPL-2.0-or-later
URL:            https://mcaster1.com
Source0:        %{name}-%{version}-alpha.tar.gz

BuildRequires:  autoconf
BuildRequires:  automake
BuildRequires:  autoconf-archive
BuildRequires:  gcc-c++
BuildRequires:  qt6-qtbase-devel >= 6.5
BuildRequires:  qt6-qtmultimedia-devel >= 6.5
BuildRequires:  qt6-qtsvg-devel
BuildRequires:  portaudio-devel
BuildRequires:  ffmpeg-devel
BuildRequires:  taglib-devel >= 1.11
BuildRequires:  sqlite-devel >= 3.24
BuildRequires:  libyaml-devel
BuildRequires:  lame-devel
BuildRequires:  mpg123-devel
BuildRequires:  libvorbis-devel
BuildRequires:  flac-devel
BuildRequires:  opus-devel
BuildRequires:  fdk-aac-free-devel
BuildRequires:  x264-devel
BuildRequires:  libvpx-devel
BuildRequires:  libtheora-devel
BuildRequires:  libass-devel
BuildRequires:  freetype-devel
BuildRequires:  pkgconfig

Requires:       qt6-qtbase >= 6.5
Requires:       qt6-qtmultimedia >= 6.5
Requires:       portaudio
Requires:       taglib >= 1.11
Requires:       sqlite >= 3.24
Requires:       libyaml

%description
Mcaster1DAWCast is a full-featured digital audio workstation designed
specifically for broadcasters, webcasters, and podcasters. It combines
multi-track audio recording and editing with video editing capabilities,
live broadcasting, podcast production tools, and broadcast graphics.

%prep
%autosetup -n %{name}-%{version}-alpha

%build
./autogen.sh
%configure
%make_build

%install
%make_install
install -Dm644 installer/linux/debian/mcaster1dawcast.desktop \
    %{buildroot}%{_datadir}/applications/mcaster1dawcast.desktop
mkdir -p %{buildroot}%{_datadir}/mcaster1dawcast
cp -r themes configs %{buildroot}%{_datadir}/mcaster1dawcast/

%files
%license LICENSE
%doc README.md PLAN.md CHANGELOG.md
%{_bindir}/mcaster1dawcast
%{_datadir}/mcaster1dawcast/
%{_datadir}/applications/mcaster1dawcast.desktop

%changelog
* Wed Apr 02 2026 David St. John <davestj@gmail.com> - 1.0.0-0.1.alpha
- Initial package
