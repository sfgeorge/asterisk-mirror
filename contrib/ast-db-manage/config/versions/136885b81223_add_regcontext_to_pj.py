"""add regcontext to pjsip

Revision ID: 136885b81223
Revises: 2d078ec071b7
Create Date: 2016-01-11 22:32:45.470522

"""

# revision identifiers, used by Alembic.
revision = '136885b81223'
down_revision = '2d078ec071b7'

from alembic import op
import sqlalchemy as sa

def upgrade():
    op.add_column('ps_globals', sa.Column('regcontext', sa.String(80)))

def downgrade():
    op.drop_column('ps_globals', 'regcontext')
