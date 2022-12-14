/*
 * Ham World Engine Editor
 * Copyright (C) 2022 Keith Hammond
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef HAM_ENGINE_EDITOR_PROJECT_HPP
#define HAM_ENGINE_EDITOR_PROJECT_HPP 1

#include <QException>
#include <QGuiApplication>
#include <QObject>
#include <QFont>
#include <QFontMetrics>
#include <QDir>
#include <QVersionNumber>
#include <QAbstractListModel>

#include "graph_editor.hpp"

#define HAM_ENGINE_EDITOR_TEMPLATE_JSON_PATH ".ham/engine-template.json"

namespace ham::engine::editor{
	QString default_project_path();

	class project_dir_error: public QException{
		public:
			void raise() const override{ throw *this; }
			project_dir_error *clone() const override{ return new project_dir_error; }
	};

	class project_json_error: public QException{
		public:
			void raise() const override{ throw *this; }
			project_json_error *clone() const override{ return new project_json_error; }
	};

	class recent_projects_model: public QAbstractListModel{
		Q_OBJECT

		public:
			explicit recent_projects_model(QObject *parent = nullptr)
				: QAbstractListModel(parent)
			{
				update_list();
			}

			int rowCount(const QModelIndex &parent = QModelIndex()) const override{
				Q_UNUSED(parent);
				return m_projs.size();
			}

			QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override{
				if(index.row() >= rowCount()){
					return QVariant();
				}

				switch(role){
					case Qt::DisplayRole: return m_projs[index.row()].first;
					case Qt::ToolTipRole: return m_projs[index.row()].second;

					case Qt::SizeHintRole:{
						const auto fnt = QGuiApplication::font();
						QFontMetrics fnt_metrics(fnt);
						const auto text_size = fnt_metrics.boundingRect(m_projs[index.row()].first).size();
						return QSize{text_size.width(), text_size.height() + 20};
					}

					default: return QVariant();
				}
			}

			void update_list();

		private:
			QList<QPair<QString, QString>> m_projs;
	};

	class project: public QObject{
		Q_OBJECT

		Q_PROPERTY(quint32 id READ id WRITE set_id NOTIFY id_changed)

		Q_PROPERTY(QDir dir READ dir NOTIFY dir_changed)

		Q_PROPERTY(QUtf8StringView name READ name WRITE set_name NOTIFY name_changed)
		Q_PROPERTY(QUtf8StringView display_name READ name WRITE set_display_name NOTIFY display_name_changed)
		Q_PROPERTY(QUtf8StringView author READ author WRITE set_author NOTIFY author_changed)
		Q_PROPERTY(QUtf8StringView license READ license WRITE set_license NOTIFY license_changed)
		Q_PROPERTY(QUtf8StringView description READ description WRITE set_description NOTIFY description_changed)

		Q_PROPERTY(QVersionNumber version READ version WRITE set_version NOTIFY version_changed)

		public:
			explicit project(const QDir &dir, QObject *parent = nullptr);
			~project();

			ham::typeset_view ts() noexcept{ return m_ts; }
			ham::const_typeset_view ts() const noexcept{ return m_ts; }

			quint32 id() const noexcept{ return m_id; }

			const QDir &dir() const noexcept{ return m_dir; }

			QUtf8StringView name() const noexcept{ return m_name; }
			QUtf8StringView display_name() const noexcept{ return m_display_name; }
			QUtf8StringView author() const noexcept{ return m_author; }
			QUtf8StringView license() const noexcept{ return m_license; }
			QUtf8StringView description() const noexcept{ return m_desc; }

			QVersionNumber version() const noexcept{ return m_ver; }

			QList<QString> graph_names() const noexcept{ return m_graphs.keys(); }

			QList<const editor::graph*> const_graphs() const noexcept{
				QList<const editor::graph*> ret;

				const auto mut = m_graphs.values();
				ret.reserve(mut.size());

				for(auto graph : mut){
					ret.append(graph);
				}

				return ret;
			}

			QList<editor::graph*> graphs() noexcept{ return m_graphs.values(); }

			editor::graph *get_graph(const QString &name) noexcept{
				return m_graphs.value(name, nullptr);
			}

			const editor::graph *get_graph(const QString &name) const noexcept{
				return m_graphs.value(name, nullptr);
			}

			void set_id(quint32 new_id) noexcept{
				if(new_id != m_id){
					m_id = new_id;
					Q_EMIT id_changed(new_id);
				}
			}

			void set_name(QUtf8StringView new_name) noexcept{
				if(new_name != m_name){
					m_name = QByteArray(new_name.data(), new_name.size());
					Q_EMIT name_changed(m_name);
				}
			}

			void set_display_name(QUtf8StringView new_display) noexcept{
				if(new_display != m_display_name){
					m_display_name = QByteArray(new_display.data(), new_display.size());
					Q_EMIT display_name_changed(m_display_name);
				}
			}

			void set_author(QUtf8StringView new_author) noexcept{
				if(new_author != m_author){
					m_author = QByteArray(new_author.data(), new_author.size());
					Q_EMIT author_changed(m_author);
				}
			}

			void set_license(QUtf8StringView new_license) noexcept{
				if(new_license != m_license){
					m_license = QByteArray(new_license.data(), new_license.size());
					Q_EMIT license_changed(m_license);
				}
			}

			void set_description(QUtf8StringView new_desc) noexcept{
				if(new_desc != m_desc){
					m_desc = QByteArray(new_desc.data(), new_desc.size());
					Q_EMIT description_changed(m_desc);
				}
			}

			void set_version(QVersionNumber new_ver) noexcept{
				if(m_ver != new_ver){
					m_ver = new_ver;
					Q_EMIT version_changed(m_ver);
				}
			}

		Q_SIGNALS:
			void id_changed(quint32);

			void dir_changed(const QDir&);

			void name_changed(QUtf8StringView);
			void display_name_changed(QUtf8StringView);
			void author_changed(QUtf8StringView);
			void license_changed(QUtf8StringView);
			void description_changed(QUtf8StringView);

			void version_changed(const QVersionNumber&);

		private:
			quint32 m_id;
			QDir m_dir;
			QByteArray m_name, m_display_name, m_author, m_license, m_desc;
			QVersionNumber m_ver;
			QHash<QString, editor::graph*> m_graphs;
			ham::typeset m_ts;
	};

	class project_template: public QObject{
		Q_OBJECT

		Q_PROPERTY(QDir dir READ dir CONSTANT)

		Q_PROPERTY(QUtf8StringView name READ name CONSTANT)
		Q_PROPERTY(QUtf8StringView author READ author CONSTANT)
		Q_PROPERTY(QUtf8StringView description READ description CONSTANT)

		public:
			explicit project_template(const QDir &dir, QObject *parent = nullptr);
			~project_template();

			const QDir &dir() const noexcept{ return m_dir; }

			QUtf8StringView name() const noexcept{ return m_name; }
			QUtf8StringView author() const noexcept{ return m_author; }
			QUtf8StringView description() const noexcept{ return m_desc; }

			bool createInDir(
				const QDir &proj_dir,
				QUtf8StringView proj_name,
				QUtf8StringView proj_display_name,
				QUtf8StringView proj_author,
				QUtf8StringView proj_description
			) const;

		private:
			QDir m_dir;
			QByteArray m_name, m_author, m_desc;
	};
}

#endif // !HAM_ENGINE_EDITOR_PROJECT_HPP
